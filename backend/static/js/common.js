function getCookie(name) {
  const prefix = name + "=";
  const parts = document.cookie.split(";");
  for (const part of parts) {
    const trimmed = part.trim();
    if (trimmed.startsWith(prefix)) {
      return decodeURIComponent(trimmed.substring(prefix.length));
    }
  }
  return "";
}

function formatErrors(data) {
  if (data && data.detail) {
    return typeof data.detail === "string" ? data.detail : JSON.stringify(data.detail);
  }
  if (data && data.errors) {
    const messages = [];
    Object.keys(data.errors).forEach(function (field) {
      const entries = data.errors[field];
      if (Array.isArray(entries)) {
        entries.forEach(function (entry) {
          messages.push(entry.message || String(entry));
        });
      }
    });
    return messages.join(" ") || "Please check the entered values.";
  }
  return "Request failed. Please try again.";
}

async function apiRequest(url, options) {
  const config = Object.assign({}, options || {});
  config.credentials = "same-origin";
  config.headers = Object.assign({}, config.headers || {});
  config.headers["X-CSRFToken"] = getCookie("csrftoken");

  if (config.body && typeof config.body !== "string") {
    config.headers["Content-Type"] = "application/json";
    config.body = JSON.stringify(config.body);
  }

  const response = await fetch(url, config);
  let data = {};
  try {
    data = await response.json();
  } catch (error) {
    data = {};
  }

  if (!response.ok) {
    const error = new Error(formatErrors(data));
    error.data = data;
    error.status = response.status;
    throw error;
  }

  return data;
}

document.addEventListener("DOMContentLoaded", function () {
  const logoutButton = document.getElementById("logout-button");
  if (!logoutButton) {
    return;
  }
  logoutButton.addEventListener("click", async function () {
    try {
      await apiRequest("/api/auth/logout/", { method: "POST", body: {} });
    } catch (error) {
      window.location.assign("/");
      return;
    }
    window.location.assign("/login/");
  });
});
