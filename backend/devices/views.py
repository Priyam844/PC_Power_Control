from django.conf import settings
from django.utils import timezone
from rest_framework import status
from rest_framework.permissions import IsAuthenticated
from rest_framework.response import Response
from rest_framework.views import APIView

from .authentication import DeviceTokenAuthentication
from .models import Command, Device
from .serializers import (
    CommandAckSerializer,
    PendingCommandSerializer,
    StatusReportSerializer,
)


class DeviceStatusView(APIView):
    authentication_classes = [DeviceTokenAuthentication]
    permission_classes = [IsAuthenticated]

    def post(self, request):
        serializer = StatusReportSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)

        device = request.user
        now = timezone.now()
        device.pc_status = serializer.validated_data["pc_status"]
        device.relay_state = serializer.validated_data["relay_state"]
        device.connection_status = Device.ConnectionStatus.ONLINE
        device.last_seen = now
        device.save(
            update_fields=[
                "pc_status",
                "relay_state",
                "connection_status",
                "last_seen",
            ]
        )

        return Response({"ok": True, "server_time": now.isoformat()})


class PendingCommandView(APIView):
    authentication_classes = [DeviceTokenAuthentication]
    permission_classes = [IsAuthenticated]

    def get(self, request):
        device = request.user
        now = timezone.now()

        Command.objects.filter(
            device=device,
            status=Command.Status.PENDING,
            expires_at__lte=now,
        ).update(status=Command.Status.EXPIRED)

        command = (
            Command.objects.filter(
                device=device,
                status__in=[
                    Command.Status.PENDING,
                    Command.Status.ACKNOWLEDGED,
                    Command.Status.FAILED,
                ],
                expires_at__gt=now,
            )
            .order_by("created_at")
            .first()
        )

        if command is None:
            Command.objects.filter(
                device=device,
                status__in=[
                    Command.Status.PENDING,
                    Command.Status.ACKNOWLEDGED,
                    Command.Status.FAILED,
                ],
                expires_at__lte=now,
            ).update(status=Command.Status.EXPIRED)
            return Response({"command": None})

        if command.status == Command.Status.PENDING:
            command.status = Command.Status.ACKNOWLEDGED
            command.acknowledged_at = now
            command.save(update_fields=["status", "acknowledged_at"])

        payload = PendingCommandSerializer(command, context={"now": now}).data
        return Response({"command": payload})


class CommandAckView(APIView):
    authentication_classes = [DeviceTokenAuthentication]
    permission_classes = [IsAuthenticated]

    def post(self, request):
        serializer = CommandAckSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)

        command = Command.objects.filter(
            command_id=serializer.validated_data["command_id"],
            device=request.user,
        ).first()
        if command is None:
            return Response(
                {"detail": "Command not found."}, status=status.HTTP_404_NOT_FOUND
            )

        new_status = serializer.validated_data["status"]
        if command.status != Command.Status.EXPIRED:
            command.status = new_status
            command.executed_at = timezone.now()
            command.result = serializer.validated_data.get("result", "")
            command.save(update_fields=["status", "executed_at", "result"])
        elif new_status == Command.Status.EXECUTED:
            return Response(
                {"detail": "Command expired."}, status=status.HTTP_400_BAD_REQUEST
            )

        return Response({"ok": True, "status": command.status})


class ServerTimeView(APIView):
    authentication_classes = [DeviceTokenAuthentication]
    permission_classes = [IsAuthenticated]

    def get(self, request):
        now = timezone.now()
        return Response(
            {
                "server_time": now.isoformat(),
                "server_epoch": int(now.timestamp()),
                "command_expiry_seconds": settings.COMMAND_EXPIRY_SECONDS,
            }
        )
