// JavaScript for Water Polo Shot Clk Page


let shotClkCnt = INITIAL_SHOT_CLK_VAL; // Set the countdown start value in seconds
let shotClkEn = SHOT_CLK_ENABLE; // Set the initial enable state
let rstShotClkCnt = RESET_SHOT_CLK_VAL; // Set the Reset state
const shotClkTimerElement = document.getElementById('shotClkTimer');


document.addEventListener("DOMContentLoaded", () => {
  document.getElementById('btnRunShotClk').addEventListener('click', toggleRunShotClk);
  document.getElementById('btnRstShotClk').addEventListener('click', rstShotClk);
  document.getElementById('btnEditShotClk').addEventListener('click', editShotClk);
});

function toggleRunShotClk() {
  shotClkEn = !shotClkEn; 
  setShotClkButtonState();
  fetch('/toggleRunShotClk');

  if (syncGameAndShotClks) {
    gameClkEn = shotClkEn;
    setGameClkButtonState();
  }
}

function rstShotClk() {
  //If the shot clock is greater than the game clock then make them equal
  if (typeof gameClkCnt === 'undefined') {
    shotClkCnt = rstShotClkCnt;
  } else {
    if(rstShotClkCnt > gameClkCnt) shotClkCnt = gameClkCnt;
    else shotClkCnt = rstShotClkCnt;
  }
  fetch('/rstShotClk');
}

function editShotClk() {
  let timeStr = prompt("Enter Shot Clock Time in seconds");
  //Check for a valid format
  if (timeStr !== null) {
    const seconds = parseInt(timeStr, 10);
    if (!isNaN(seconds) && seconds >= 0) {
      // Convert to milliseconds and update the countdown
      const totalMilliseconds = seconds * 1000;
      shotClkCnt = totalMilliseconds;
      fetch(`/editShotClk?time=${totalMilliseconds}`);
    } else {
      alert("Invalid time. Please enter a valid time in ss format.");
    }
  }
}

function setShotClkButtonState() {
  const btnRunShotClk = document.getElementById('btnRunShotClk');
  if (shotClkEn) {
    btnRunShotClk.style.backgroundColor = '#c9535a';
    btnRunShotClk.textContent = 'Stop';
  } else {
    btnRunShotClk.style.backgroundColor = '#63de3e';
    btnRunShotClk.textContent = 'Run';
  }
}