import hmac

from rest_framework.authentication import BaseAuthentication
from rest_framework.exceptions import AuthenticationFailed

from .models import Device


class DeviceTokenAuthentication(BaseAuthentication):
    keyword = "Token"

    def authenticate(self, request):
        header = request.META.get("HTTP_AUTHORIZATION", "")
        parts = header.split()
        if len(parts) != 2 or parts[0] != self.keyword:
            return None

        token = parts[1]
        if not token:
            raise AuthenticationFailed("Invalid device token.")

        digest = Device.hash_token(token)
        try:
            device = Device.objects.get(device_token_hash=digest)
        except Device.DoesNotExist:
            raise AuthenticationFailed("Invalid device token.")

        if not hmac.compare_digest(device.device_token_hash, digest):
            raise AuthenticationFailed("Invalid device token.")

        return device, token

    def authenticate_header(self, request):
        return self.keyword
