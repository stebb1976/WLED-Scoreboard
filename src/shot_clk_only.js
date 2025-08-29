// JavaScript for Water Polo Game Clk Page

//Create internal timers to update the display
const interval = setInterval(() => {
  // Shot Clock Timer
  if(shotClkEn) {  
    shotClkCnt = shotClkCnt - 100; // Decrease countdown by 100 milliseconds
    if (shotClkCnt < 0) shotClkCnt = 0;
  } 
  shotClkTimerElement.textContent = Math.round(shotClkCnt/1000); // Update the timer display
}, 100); // Update every second

document.addEventListener("DOMContentLoaded", () => {
  document.getElementById('btnHome').addEventListener('click', () => { location.href = './'; });
});