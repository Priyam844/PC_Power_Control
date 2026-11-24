from django.urls import path

from .views import (
    CommandAckView,
    DeviceStatusView,
    PendingCommandView,
    ServerTimeView,
)

urlpatterns = [
    path("status/", DeviceStatusView.as_view(), name="device-status"),
    path("commands/", PendingCommandView.as_view(), name="device-commands"),
    path("commands/ack/", CommandAckView.as_view(), name="device-command-ack"),
    path("time/", ServerTimeView.as_view(), name="device-time"),
]
