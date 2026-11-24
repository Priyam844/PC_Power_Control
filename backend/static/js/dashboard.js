(function () {
  const pcState = document.getElementById("pc-state");
  const pcCaption = document.getElementById("pc-caption");
  const stateTag = document.getElementById("state-tag");
  const deviceState = document.getElementById("device-state");
  const deviceDot = document.getElementById("device-dot");
  const deviceName = document.getElementById("device-name");
  const lastSeen = document.getElementById("last-seen");
  const relayState = document.getElementById("relay-state");
  const relayDot = document.getElementById("relay-dot");
  const chip = document.getElementById("device-chip");
  const chipDot = document.getElementById("chip-dot");
  const chipLabel = document.getElementById("chip-label");
  const signal = document.getElementById("signal");
  const slidePower = document.getElementById("slide-power");
  const slideFill = document.getElementById("slide-fill");
  const slideLabel = document.getElementById("slide-label");
  const slideKnob = document.getElementById("slide-knob");
  const feedback = document.getElementById("feedback");
  const commandState = document.getElementById("command-state");

  signal.innerHTML = Array.from({ length: 24 }, function (_, index) {
    const height = 30 + Math.round(Math.abs(Math.sin(index * 0.7)) * 62);
    return '<span style="--i:' + index + ';--h:' + height + '%"></span>';
  }).join("");

  let busy = false;
  let online = false;
  let pending = false;
  let pcStatusValue = "UNKNOWN";
  let dragging = false;
  let dragStartX = 0;
  let dragProgress = 0;
  let slideArmed = false;
  let slideTimer = null;

  function message(text, type) {
    feedback.className = "feedback" + (type ? " is-" + type : "");
    feedback.textContent = text || "";
  }

  function setState(element, value, mode) {
    element.textContent = value;
    element.className = "state-value state-" + mode;
  }

  function captionFor(value) {
    if (!online) {
      return "Status is unavailable while the device is offline.";
    }
    if (value === "ON") {
      return "The PC is running.";
    }
    if (value === "OFF") {
      return "The PC is powered down.";
    }
    return "Waiting for a valid status report from the sensor.";
  }

  function setSignal(connected, pcStatus) {
    let nextClass = "is-standby";
    if (!connected) {
      nextClass = "is-offline";
    } else if (pcStatus === "ON") {
      nextClass = "is-on";
    } else if (pcStatus === "OFF") {
      nextClass = "is-off";
    }
    if (signal.classList.contains(nextClass)) {
      return;
    }
    signal.classList.remove("is-offline", "is-off", "is-standby", "is-on");
    void signal.offsetWidth;
    signal.classList.add(nextClass);
  }

  function actionLabel() {
    if (!online) {
      return "Device offline";
    }
    return pcStatusValue === "ON" ? "Slide to power off" : "Slide to power on";
  }

  function confirmLabel() {
    return pcStatusValue === "ON" ? "Press again to power off" : "Press again to power on";
  }

  function showDisconnected() {
    pcStatusValue = "UNKNOWN";
    setState(pcState, "UNKNOWN", "unknown");
    stateTag.textContent = "UNKNOWN";
    stateTag.className = "tag is-unknown";
    pcCaption.textContent = "Status is unavailable while the device is offline.";
    deviceState.textContent = "OFFLINE";
    deviceDot.className = "dot is-off";
    chip.className = "device-chip is-offline";
    chipDot.className = "dot is-off";
    chipLabel.textContent = "Device offline";
    setSignal(false, null);
    resetSlide();
  }

  function formatTime(value) {
    if (!value) {
      return "Never";
    }
    const date = new Date(value);
    if (Number.isNaN(date.getTime())) {
      return "Never";
    }
    return date.toLocaleString();
  }

  function slideMax() {
    return Math.max(1, slidePower.clientWidth - slideKnob.offsetWidth - 10);
  }

  function renderSlide(progress) {
    const travel = Math.round(slideMax() * progress);
    slideKnob.style.transform = "translateX(" + travel + "px)";
    slideFill.style.width = slideKnob.offsetWidth + 10 + travel + "px";
    slideLabel.style.opacity = String(Math.max(0.25, 1 - progress * 1.35));
  }

  function disarmSlide() {
    slideArmed = false;
    if (slideTimer !== null) {
      clearTimeout(slideTimer);
      slideTimer = null;
    }
    if (!dragging) {
      slideLabel.textContent = actionLabel();
    }
  }

  function resetSlide() {
    dragging = false;
    dragProgress = 0;
    slidePower.classList.remove("is-dragging");
    disarmSlide();
    slideLabel.textContent = actionLabel();
    renderSlide(0);
  }

  function updateButton() {
    const disabled = busy || pending || !online;
    const powerOff = online && pcStatusValue === "ON";

    slidePower.classList.toggle("is-power-on", !powerOff);
    slidePower.classList.toggle("is-power-off", powerOff);
    slidePower.classList.toggle("is-disabled", disabled);
    slidePower.setAttribute("aria-disabled", String(disabled));
    slideKnob.disabled = disabled;

    if (disabled) {
      resetSlide();
    }

    if (busy) {
      slideLabel.textContent = "Queuing...";
    } else if (pending) {
      slideLabel.textContent = "Command pending";
    } else if (!slideArmed) {
      slideLabel.textContent = actionLabel();
    }

    slideKnob.setAttribute(
      "aria-label",
      disabled ? "Power control unavailable" : powerOff ? "Slide to power off" : "Slide to power on"
    );
  }

  async function queueCommand() {
    if (busy || pending || !online) {
      return;
    }
    busy = true;
    updateButton();
    message("Queuing power-button press...", "info");

    try {
      await apiRequest("/api/pc/power/", { method: "POST", body: {} });
      message("Command queued. Waiting for the ESP8266.", "success");
    } catch (error) {
      message(error.message, "error");
    } finally {
      busy = false;
      resetSlide();
      await refresh();
      setTimeout(refresh, 1200);
    }
  }

  function finishDrag(event) {
    if (!dragging) {
      return;
    }
    dragging = false;
    slidePower.classList.remove("is-dragging");
    if (event && slideKnob.hasPointerCapture && slideKnob.hasPointerCapture(event.pointerId)) {
      slideKnob.releasePointerCapture(event.pointerId);
    }
    if (dragProgress >= 0.82) {
      renderSlide(1);
      queueCommand();
    } else {
      renderSlide(0);
    }
  }

  slideKnob.addEventListener("pointerdown", function (event) {
    if (slidePower.classList.contains("is-disabled")) {
      return;
    }
    event.preventDefault();
    dragging = true;
    dragStartX = event.clientX;
    disarmSlide();
    slidePower.classList.add("is-dragging");
    slideKnob.setPointerCapture(event.pointerId);
  });

  slideKnob.addEventListener("pointermove", function (event) {
    if (!dragging) {
      return;
    }
    const delta = event.clientX - dragStartX;
    dragProgress = Math.min(1, Math.max(0, delta / slideMax()));
    renderSlide(dragProgress);
  });

  slideKnob.addEventListener("pointerup", finishDrag);
  slideKnob.addEventListener("pointercancel", finishDrag);

  slideKnob.addEventListener("keydown", function (event) {
    if (slidePower.classList.contains("is-disabled")) {
      return;
    }
    if (event.key === "Escape") {
      disarmSlide();
      renderSlide(0);
      return;
    }
    if (event.key === "ArrowRight") {
      event.preventDefault();
      dragProgress = Math.min(1, dragProgress + 0.25);
      renderSlide(dragProgress);
      return;
    }
    if (event.key === "ArrowLeft") {
      event.preventDefault();
      dragProgress = Math.max(0, dragProgress - 0.25);
      renderSlide(dragProgress);
      return;
    }
    if (event.key === "Enter" || event.key === " ") {
      event.preventDefault();
      if (slideArmed || dragProgress >= 0.82) {
        disarmSlide();
        renderSlide(1);
        queueCommand();
      } else {
        slideArmed = true;
        slideLabel.textContent = confirmLabel();
        if (slideTimer !== null) {
          clearTimeout(slideTimer);
        }
        slideTimer = setTimeout(function () {
          disarmSlide();
          renderSlide(0);
        }, 3000);
      }
    }
  });

  slideKnob.addEventListener("blur", function () {
    disarmSlide();
    if (!dragging) {
      renderSlide(0);
    }
  });

  async function refresh() {
    try {
      const data = await apiRequest("/api/pc/status/");
      if (!data.pc_status || !data.device_status) {
        throw new Error("Session expired. Sign in again.");
      }
      online = data.device_status === "ONLINE";
      pending = data.pending_command;
      pcStatusValue = data.pc_status;

      const mode = data.pc_status === "ON" ? "on" : data.pc_status === "OFF" ? "off" : "unknown";
      setState(pcState, data.pc_status, mode);
      stateTag.textContent = data.pc_status;
      stateTag.className = "tag is-" + mode;
      pcCaption.textContent = captionFor(data.pc_status);

      deviceName.textContent = data.device_name || "Not registered";
      deviceState.textContent = data.device_status;
      deviceDot.className = "dot " + (online ? "is-on" : "is-off");
      lastSeen.textContent = formatTime(data.last_seen);
      relayState.textContent = data.relay_state;
      relayDot.className = "dot " + (data.relay_state === "ACTIVE" ? "is-warn" : "is-off");

      chip.className = "device-chip " + (online ? "is-online" : "is-offline");
      chipDot.className = "dot " + (online ? "is-on" : "is-off");
      chipLabel.textContent = online ? "Device online" : "Device offline";
      setSignal(online, data.pc_status);

      if (pending) {
        commandState.textContent = "Waiting for the ESP8266 to acknowledge the command...";
      } else if (data.last_command_status === "EXPIRED") {
        commandState.textContent = "Last command expired before the ESP8266 claimed it.";
      } else if (data.last_command_status === "EXECUTED") {
        commandState.textContent = "Last power-button press completed.";
      } else {
        commandState.textContent = "No active command.";
      }
      updateButton();
    } catch (error) {
      online = false;
      pending = false;
      showDisconnected();
      updateButton();
      message(error.message, "error");
    }
  }

  setInterval(refresh, 3000);
  refresh();
})();
