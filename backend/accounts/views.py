import secrets
from datetime import timedelta

from django.conf import settings
from django.contrib.auth import authenticate, login, logout
from django.contrib.auth.decorators import login_required, user_passes_test
from django.contrib.auth.views import LoginView
from django.shortcuts import render
from django.urls import reverse
from django.utils import timezone
from rest_framework import status
from rest_framework.permissions import AllowAny, IsAuthenticated
from rest_framework.response import Response
from rest_framework.views import APIView

from devices.models import (
    DEFAULT_DEVICE_NAME,
    DEVICE_TOKEN_SESSION_KEY,
    Command,
    Device,
)

from .forms import CredentialChangeForm
from .serializers import LoginSerializer


class LoginAPIView(APIView):
    authentication_classes = []
    permission_classes = [AllowAny]
    throttle_scope = "login"

    def post(self, request):
        serializer = LoginSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)

        user = authenticate(
            request,
            username=serializer.validated_data["username"],
            password=serializer.validated_data["password"],
        )
        if user is None or not user.is_active:
            return Response(
                {"detail": "Invalid username or password."},
                status=status.HTTP_401_UNAUTHORIZED,
            )

        login(request, user)
        return Response({"ok": True, "detail": "Signed in."})


class LogoutAPIView(APIView):
    permission_classes = [IsAuthenticated]

    def post(self, request):
        logout(request)
        return Response({"ok": True})


class ChangeCredentialsAPIView(APIView):
    permission_classes = [IsAuthenticated]

    def post(self, request):
        form = CredentialChangeForm(request.user, data=request.data)
        if not form.is_valid():
            return Response(
                {"errors": form.errors.get_json_data()},
                status=status.HTTP_400_BAD_REQUEST,
            )

        user = request.user
        new_username = form.cleaned_data.get("new_username")
        if new_username:
            user.username = new_username
        user.set_password(form.cleaned_data["new_password"])
        user.save()
        logout(request)

        return Response(
            {"ok": True, "detail": "Credentials updated. Please sign in again."}
        )


class PCStatusAPIView(APIView):
    def get(self, request):
        now = timezone.now()
        device = Device.objects.order_by("id").first()

        if device is None:
            return Response(
                {
                    "username": request.user.username,
                    "device_name": None,
                    "device_status": Device.ConnectionStatus.OFFLINE,
                    "pc_status": Device.PCStatus.UNKNOWN,
                    "last_seen": None,
                    "relay_state": Device.RelayState.INACTIVE,
                    "pending_command": False,
                    "last_command_status": None,
                }
            )

        device.commands.filter(
            status=Command.Status.PENDING, expires_at__lte=now
        ).update(status=Command.Status.EXPIRED)
        pending = device.commands.filter(
            status__in=[Command.Status.PENDING, Command.Status.ACKNOWLEDGED],
            expires_at__gt=now,
        ).exists()
        last_command = device.commands.order_by("-created_at").first()

        return Response(
            {
                "username": request.user.username,
                "device_name": device.device_name,
                "device_status": device.effective_connection_status(now),
                "pc_status": device.effective_pc_status(now),
                "last_seen": device.last_seen.isoformat() if device.last_seen else None,
                "relay_state": device.relay_state,
                "pending_command": pending,
                "last_command_status": (
                    last_command.status if last_command is not None else None
                ),
            }
        )


class PowerAPIView(APIView):
    throttle_scope = "power"

    def post(self, request):
        device = Device.objects.order_by("id").first()
        if device is None:
            return Response(
                {"detail": "No device is registered."},
                status=status.HTTP_503_SERVICE_UNAVAILABLE,
            )

        now = timezone.now()
        device.commands.filter(
            status=Command.Status.PENDING, expires_at__lte=now
        ).update(status=Command.Status.EXPIRED)

        already_pending = device.commands.filter(
            status__in=[Command.Status.PENDING, Command.Status.ACKNOWLEDGED],
            expires_at__gt=now,
        ).exists()
        if already_pending:
            return Response(
                {"detail": "A command is already pending."},
                status=status.HTTP_409_CONFLICT,
            )

        command = Command.objects.create(
            device=device,
            command=Command.Type.PRESS_POWER,
            expires_at=now + timedelta(seconds=settings.COMMAND_EXPIRY_SECONDS),
        )
        return Response(
            {
                "command_id": str(command.command_id),
                "status": command.status,
                "expires_in_seconds": settings.COMMAND_EXPIRY_SECONDS,
            },
            status=status.HTTP_201_CREATED,
        )


@login_required
def dashboard(request):
    return render(request, "dashboard.html")


@login_required
def settings_page(request):
    return render(request, "settings.html")


@user_passes_test(
    lambda user: user.is_staff or user.is_superuser, login_url="login"
)
def device_token_page(request):
    device = Device.objects.order_by("id").first()
    token = request.session.pop(DEVICE_TOKEN_SESSION_KEY, None)

    if request.method == "POST":
        token = secrets.token_urlsafe(32)
        if device is None:
            Device.objects.create(
                device_name=DEFAULT_DEVICE_NAME,
                device_token_hash=Device.hash_token(token),
            )
        else:
            device.device_token_hash = Device.hash_token(token)
            device.save(update_fields=["device_token_hash"])
        request.session.pop(DEVICE_TOKEN_SESSION_KEY, None)

    return render(request, "device_token.html", {"device": device, "token": token})


class ConsoleLoginView(LoginView):
    template_name = "login.html"
    redirect_authenticated_user = True

    def get_success_url(self):
        if DEVICE_TOKEN_SESSION_KEY in self.request.session:
            return reverse("device-token")
        return super().get_success_url()


website_login = ConsoleLoginView.as_view()
