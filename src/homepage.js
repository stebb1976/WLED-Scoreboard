// JavaScript for Water Polo Scoreboard Home Page

let displayOn = INITIAL_DISPLAY_ON_STATE; // Set the initial display state

function toggleDisplayOn() {
  displayOn = !displayOn;
  setDisplayOnButtonState();
  fetch('/toggleDisplayOn');
}

function setDisplayOnButtonState() {
  const btnLEDToggle = document.getElementById('btnLEDToggle');
  if (displayOn) {
    btnLEDToggle.style.backgroundColor = '#63de3e';
    btnLEDToggle.textContent = 'Display On';
  } else {
    btnLEDToggle.style.backgroundColor = '#c9535a';
    btnLEDToggle.textContent = 'Display Off';
  }
}

document.addEventListener("DOMContentLoaded", () => {
    document.getElementById('btnLEDToggle').addEventListener('click', toggleDisplayOn);
});