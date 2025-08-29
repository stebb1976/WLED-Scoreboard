// JavaScript for Water Polo Game Clk Page

let gameClkCnt = INITIAL_GAME_CLK_VAL; // Set the countdown start value in milliseconds
let gameClkEn = GAME_CLK_ENABLE; // Set the initial enable state
let rstGameClkCnt = RESET_GAME_CLK_VAL; // Set the Reset state
const gameTimerElement = document.getElementById('gameClkTimer');

document.addEventListener("DOMContentLoaded", () => {
  document.getElementById('btnRunGameClk').addEventListener('click', toggleRunGameClk);
  document.getElementById('btnRstGameClk').addEventListener('click', rstGameClk);
  document.getElementById('btnEditGameClk').addEventListener('click', editGameClk);
});
    
function toggleRunGameClk() {
  gameClkEn = !gameClkEn;
  setGameClkButtonState();
  fetch('/toggleRunGameClk'); //This handles the LED Matrix update

  if (syncGameAndShotClks) {
    shotClkEn = gameClkEn;
    setShotClkButtonState();
  }
}

function rstGameClk() {
  gameClkCnt = rstGameClkCnt;
  fetch('/rstGameClk');
}

function editGameClk() {
  let timeStr = prompt("Enter Game Clock Time in m:ss");
  //Check for a valid format
  if (timeStr !== null) {
    const timeParts = timeStr.split(":");
    if (timeParts.length === 2) {
      const minutes = parseInt(timeParts[0], 10);
      const seconds = parseInt(timeParts[1], 10);
      if (!isNaN(minutes) && !isNaN(seconds) && minutes >= 0 && seconds >= 0 && seconds < 60) {
        // Convert to milliseconds and update the countdown
        const totalMilliseconds = (minutes * 60 + seconds) * 1000;
        gameClkCnt = totalMilliseconds;
        fetch(`/editGameClk?time=${totalMilliseconds}`);
      } else {
        alert("Invalid time. Please enter a valid time in m::ss format.");
      }
    } else {
      alert("Invalid format. Please use m::ss format.");
    }
  }
}

function setGameClkButtonState() {
  const btnRunGameClk = document.getElementById('btnRunGameClk');
  if (gameClkEn) {
    btnRunGameClk.style.backgroundColor = '#c9535a';
    btnRunGameClk.textContent = 'Stop';
  } else {
    btnRunGameClk.style.backgroundColor = '#63de3e';
    btnRunGameClk.textContent = 'Run';
  }
}