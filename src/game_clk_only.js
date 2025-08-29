// JavaScript for Water Polo Game Clk Page

//Create internal timers to update the display
const interval = setInterval(() => {
  // Game Clock Timer
  if(gameClkEn) {
    gameClkCnt = gameClkCnt - 100; // Decrease countdown by 10 milliseconds
    if (gameClkCnt < 0)  gameClkCnt = 0;
  } 
  gameTimerElement.textContent = formatTime(Math.round(gameClkCnt/1000)); // Update the timer display
}, 100); // Update every second

document.addEventListener("DOMContentLoaded", () => {
  document.getElementById('btnHome').addEventListener('click', () => { location.href = './'; });
});