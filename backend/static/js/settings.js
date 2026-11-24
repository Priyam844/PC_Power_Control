(function () {
  const form = document.getElementById("settings-form");
  const feedback = document.getElementById("settings-feedback");

  function message(text, type) {
    feedback.className = "feedback is-" + type;
    feedback.textContent = text;
  }

  form.addEventListener("submit", async function (event) {
    event.preventDefault();
    const payload = {
      current_password: document.getElementById("current-password").value,
      new_username: document.getElementById("new-username").value,
      new_password: document.getElementById("new-password").value,
      confirm_password: document.getElementById("confirm-password").value,
    };

    try {
      const data = await apiRequest("/api/auth/change-credentials/", {
        method: "POST",
        body: payload,
      });
      window.setTimeout(function () {
        window.location.assign("/login/");
      }, 1200);
      message(data.detail, "success");
    } catch (error) {
      message(error.message, "danger");
    }
  });
})();
