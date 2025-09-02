// JavaScript for Pace Clk Page


let paceClkCnt = INITIAL_PACE_CLK_VAL; // Set the countdown start value in seconds
let paceClkEn = PACE_CLK_ENABLE; // Set the initial enable state
let rstPaceClkCnt = RESET_PACE_CLK_VAL; // Set the Reset state
const paceClkTimerElement = document.getElementById('paceClkTimer');
let paceCountdown = INITIAL_PACE_COUNTDOWN_STATE;

//Create internal timers to update the display
const interval = setInterval(() => {
  // Shot Clock Timer
  if(paceClkEn) {  
    paceClkCnt = paceClkCnt - 100; // Decrease countdown by 100 milliseconds
    if (paceClkCnt < 0) paceClkCnt = 0;
  } 
  paceClkTimerElement.textContent = Math.round(paceClkCnt/1000); // Update the timer display
}, 100); // Update every second

document.addEventListener("DOMContentLoaded", () => {
  document.getElementById('btnHome').addEventListener('click', () => { location.href = './'; });
  document.getElementById('btnRunPaceClk').addEventListener('click', toggleRunPaceClk);
  document.getElementById('btnRstPaceClk').addEventListener('click', rstPaceClk);
  document.getElementById('btnEditPaceClk').addEventListener('click', editPaceClk);
  document.getElementById('checkboxPaceCountdown').addEventListener('click', togglePaceCountdown);
});

function toggleRunPaceClk() {
  paceClkEn = !paceClkEn; 
  setPaceClkButtonState();
  fetch('/toggleRunPaceClk');
}

function rstPaceClk() {
  //If the pace clock is greater than the game clock then make them equal
  paceClkCnt = rstPaceClkCnt;
  fetch('/rstPaceClk');
}

function editPaceClk() {
  let timeStr = prompt("Enter pace Clock Time in seconds");
  //Check for a valid format
  if (timeStr !== null) {
    const seconds = parseInt(timeStr, 10);
    if (!isNaN(seconds) && seconds >= 0) {
      // Convert to milliseconds and update the countdown
      const totalMilliseconds = seconds * 1000;
      paceClkCnt = totalMilliseconds;
      fetch(`/editPaceClk?time=${totalMilliseconds}`);
    } else {
      alert("Invalid time. Please enter a valid time in ss format.");
    }
  }
}

function setPaceClkButtonState() {
  const btnRunPaceClk = document.getElementById('btnRunPaceClk');
  if (paceClkEn) {
    btnRunPaceClk.style.backgroundColor = '#c9535a';
    btnRunPaceClk.textContent = 'Stop';
  } else {
    btnRunPaceClk.style.backgroundColor = '#63de3e';
    btnRunPaceClk.textContent = 'Run';
  }
}

function togglePaceCountdown() {
  paceCountdown = !paceCountdown; 
  setPaceCountdownButtonState();
  fetch('/togglePaceCountdown');
}