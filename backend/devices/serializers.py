from rest_framework import serializers

from .models import Command, Device


class StatusReportSerializer(serializers.Serializer):
    pc_status = serializers.ChoiceField(
        choices=Device.PCStatus.choices, default=Device.PCStatus.UNKNOWN, required=False
    )
    relay_state = serializers.ChoiceField(
        choices=Device.RelayState.choices,
        default=Device.RelayState.INACTIVE,
        required=False,
    )


class CommandAckSerializer(serializers.Serializer):
    command_id = serializers.UUIDField()
    status = serializers.ChoiceField(
        choices=[Command.Status.EXECUTED, Command.Status.FAILED]
    )
    result = serializers.CharField(max_length=100, required=False, allow_blank=True)


class PendingCommandSerializer(serializers.ModelSerializer):
    command_id = serializers.UUIDField(format="hex_verbose")
    expires_in_seconds = serializers.SerializerMethodField()

    class Meta:
        model = Command
        fields = ["command_id", "command", "expires_in_seconds"]

    def get_expires_in_seconds(self, obj):
        now = self.context.get("now")
        seconds = (obj.expires_at - now).total_seconds()
        return max(0, int(seconds))
