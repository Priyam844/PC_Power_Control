import hashlib
import secrets
import uuid

from django.conf import settings
from django.contrib.auth.signals import user_logged_in
from django.db import models
from django.dispatch import receiver
from django.utils import timezone

DEFAULT_DEVICE_NAME = "pc-controller-01"
DEVICE_TOKEN_SESSION_KEY = "device_token_once"


class Device(models.Model):
    class PCStatus(models.TextChoices):
        ON = "ON", "ON"
        OFF = "OFF", "OFF"
        UNKNOWN = "UNKNOWN", "UNKNOWN"

    class ConnectionStatus(models.TextChoices):
        ONLINE = "ONLINE", "ONLINE"
        OFFLINE = "OFFLINE", "OFFLINE"

    class RelayState(models.TextChoices):
        ACTIVE = "ACTIVE", "ACTIVE"
        INACTIVE = "INACTIVE", "INACTIVE"

    device_name = models.CharField(max_length=100, unique=True)
    device_token_hash = models.CharField(max_length=64, unique=True)
    pc_status = models.CharField(
        max_length=7, choices=PCStatus.choices, default=PCStatus.UNKNOWN
    )
    connection_status = models.CharField(
        max_length=7,
        choices=ConnectionStatus.choices,
        default=ConnectionStatus.OFFLINE,
    )
    relay_state = models.CharField(
        max_length=8,
        choices=RelayState.choices,
        default=RelayState.INACTIVE,
    )
    last_seen = models.DateTimeField(null=True, blank=True)
    created_at = models.DateTimeField(auto_now_add=True)

    class Meta:
        ordering = ["id"]

    @property
    def is_authenticated(self):
        return True

    @staticmethod
    def hash_token(token):
        return hashlib.sha256(token.encode("utf-8")).hexdigest()

    def is_online(self, now=None):
        if not self.last_seen:
            return False
        now = now or timezone.now()
        return (now - self.last_seen).total_seconds() <= settings.DEVICE_ONLINE_SECONDS

    def effective_pc_status(self, now=None):
        if not self.is_online(now):
            return self.PCStatus.UNKNOWN
        return self.pc_status

    def effective_connection_status(self, now=None):
        if self.is_online(now):
            return self.ConnectionStatus.ONLINE
        return self.ConnectionStatus.OFFLINE

    def __str__(self):
        return self.device_name


class Command(models.Model):
    class Status(models.TextChoices):
        PENDING = "PENDING", "PENDING"
        ACKNOWLEDGED = "ACKNOWLEDGED", "ACKNOWLEDGED"
        EXECUTED = "EXECUTED", "EXECUTED"
        FAILED = "FAILED", "FAILED"
        EXPIRED = "EXPIRED", "EXPIRED"

    class Type(models.TextChoices):
        PRESS_POWER = "PRESS_POWER", "PRESS_POWER"

    command_id = models.UUIDField(default=uuid.uuid4, unique=True, editable=False)
    device = models.ForeignKey(Device, on_delete=models.CASCADE, related_name="commands")
    command = models.CharField(max_length=50, choices=Type.choices, default=Type.PRESS_POWER)
    status = models.CharField(max_length=20, choices=Status.choices, default=Status.PENDING)
    created_at = models.DateTimeField(auto_now_add=True)
    acknowledged_at = models.DateTimeField(null=True, blank=True)
    executed_at = models.DateTimeField(null=True, blank=True)
    expires_at = models.DateTimeField()
    result = models.CharField(max_length=100, blank=True)

    class Meta:
        ordering = ["created_at"]

    def __str__(self):
        return f"{self.command_id} {self.command} {self.status}"


@receiver(user_logged_in)
def issue_initial_device_token(sender, request, user, **kwargs):
    if not user.is_staff and not user.is_superuser:
        return
    if Device.objects.filter(device_name=DEFAULT_DEVICE_NAME).exists():
        return

    token = secrets.token_urlsafe(32)
    _, created = Device.objects.get_or_create(
        device_name=DEFAULT_DEVICE_NAME,
        defaults={"device_token_hash": Device.hash_token(token)},
    )
    if created:
        request.session[DEVICE_TOKEN_SESSION_KEY] = token
