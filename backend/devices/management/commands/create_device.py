import secrets

from django.core.management.base import BaseCommand

from devices.models import Device


class Command(BaseCommand):
    help = "Register a device and print its one-time token"

    def add_arguments(self, parser):
        parser.add_argument("--name", default="pc-controller-01")

    def handle(self, *args, **options):
        name = options["name"]
        token = secrets.token_urlsafe(32)
        device, created = Device.objects.get_or_create(
            device_name=name,
            defaults={"device_token_hash": Device.hash_token(token)},
        )
        if not created:
            device.device_token_hash = Device.hash_token(token)
            device.save(update_fields=["device_token_hash"])

        self.stdout.write(self.style.SUCCESS(f"Device: {device.device_name}"))
        self.stdout.write(self.style.WARNING(f"Device token: {token}"))
        self.stdout.write("Store this token in firmware/esp8266/esp8266.ino; it is not shown again.")
