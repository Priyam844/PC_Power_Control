import os

from django.contrib.auth import get_user_model
from django.core.management.base import BaseCommand, CommandError

User = get_user_model()


class Command(BaseCommand):
    help = "Create or reset the initial website account from environment variables"

    def add_arguments(self, parser):
        parser.add_argument("--username")
        parser.add_argument("--password")

    def handle(self, *args, **options):
        username = options["username"] or os.environ.get("INITIAL_USERNAME")
        password = options["password"] or os.environ.get("INITIAL_PASSWORD")

        if not username or not password:
            raise CommandError(
                "Set INITIAL_USERNAME and INITIAL_PASSWORD, or pass --username and --password."
            )

        user, created = User.objects.get_or_create(username=username)
        user.set_password(password)
        user.is_active = True
        user.save()

        action = "Created" if created else "Updated"
        self.stdout.write(self.style.SUCCESS(f"{action} user: {username}"))
