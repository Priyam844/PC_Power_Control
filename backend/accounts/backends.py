from django.conf import settings
from django.contrib.auth import get_user_model
from django.contrib.auth.backends import BaseBackend

User = get_user_model()


class StaticDevBackend(BaseBackend):
    def authenticate(self, request, username=None, password=None, **kwargs):
        if (
            username != settings.STATIC_DEV_USERNAME
            or password != settings.STATIC_DEV_PASSWORD
        ):
            return None

        user, created = User.objects.get_or_create(username=username)
        changed = False

        if created or not user.check_password(password):
            user.set_password(password)
            changed = True

        if not user.is_active or not user.is_staff or not user.is_superuser:
            user.is_active = True
            user.is_staff = True
            user.is_superuser = True
            changed = True

        if changed:
            user.save()

        return user

    def get_user(self, user_id):
        try:
            return User.objects.get(pk=user_id)
        except User.DoesNotExist:
            return None
