from django.urls import path

from .views import (
    ChangeCredentialsAPIView,
    LoginAPIView,
    LogoutAPIView,
    PCStatusAPIView,
    PowerAPIView,
    dashboard,
    device_token_page,
    settings_page,
    website_login,
)

urlpatterns = [
    path("", dashboard, name="dashboard"),
    path("login/", website_login, name="login"),
    path("settings/", settings_page, name="settings"),
    path("device-token/", device_token_page, name="device-token"),
    path("api/auth/login/", LoginAPIView.as_view(), name="api-login"),
    path("api/auth/logout/", LogoutAPIView.as_view(), name="api-logout"),
    path(
        "api/auth/change-credentials/",
        ChangeCredentialsAPIView.as_view(),
        name="api-change-credentials",
    ),
    path("api/pc/status/", PCStatusAPIView.as_view(), name="api-pc-status"),
    path("api/pc/power/", PowerAPIView.as_view(), name="api-pc-power"),
]
