// JavaScript for Water Polo Scoreboard Utilities Functions

function formatTime(seconds) {
  const minutes = Math.floor(seconds / 60);
  const remainingSeconds = seconds % 60;
  return String(minutes).padStart(2, '0') + ":" + String(remainingSeconds).padStart(2, '0');
}

function isNumber(str) {
    return !isNaN(str) && str.trim() !== ""; // Ensure it's not empty or whitespace
}