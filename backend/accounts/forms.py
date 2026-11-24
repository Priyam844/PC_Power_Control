from django import forms
from django.contrib.auth import get_user_model
from django.contrib.auth.password_validation import validate_password
from django.core.exceptions import ValidationError

User = get_user_model()


class CredentialChangeForm(forms.Form):
    current_password = forms.CharField(widget=forms.PasswordInput)
    new_username = forms.CharField(max_length=150, required=False)
    new_password = forms.CharField(widget=forms.PasswordInput)
    confirm_password = forms.CharField(widget=forms.PasswordInput)

    def __init__(self, user, *args, **kwargs):
        self.user = user
        super().__init__(*args, **kwargs)

    def clean_current_password(self):
        value = self.cleaned_data["current_password"]
        if not self.user.check_password(value):
            raise forms.ValidationError("Current password is incorrect.")
        return value

    def clean_new_username(self):
        value = self.cleaned_data.get("new_username", "").strip()
        if value and (
            User.objects.filter(username__iexact=value)
            .exclude(pk=self.user.pk)
            .exists()
        ):
            raise forms.ValidationError("That username is already in use.")
        return value

    def clean(self):
        cleaned_data = super().clean()
        password = cleaned_data.get("new_password")
        confirmation = cleaned_data.get("confirm_password")

        if password and confirmation and password != confirmation:
            self.add_error("confirm_password", "Passwords do not match.")

        if password:
            try:
                validate_password(password, self.user)
            except ValidationError as exc:
                self.add_error("new_password", exc.messages)

        return cleaned_data
