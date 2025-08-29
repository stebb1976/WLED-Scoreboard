// JavaScript for Water Polo Scoreboard Game & Shot Clk Page

let gameClkCnt = INITIAL_GAME_CLK_VAL; // Set the countdown start value in milliseconds
let gameClkEn = GAME_CLK_ENABLE; // Set the initial enable state
let rstGameClkCnt = RESET_GAME_CLK_VAL; // Set the Reset state
const gameTimerElement = document.getElementById('gameClkTimer');

let shotClkCnt = INITIAL_SHOT_CLK_VAL; // Set the countdown start value in seconds
let shotClkEn = SHOT_CLK_ENABLE; // Set the initial enable state
let rstShotClkCnt = RESET_SHOT_CLK_VAL; // Set the Reset state
const shotClkTimerElement = document.getElementById('shotClkTimer');

let syncGameAndShotClks = SYNC_CLKS; // Set the initial sync state 

//Create internal timers to update the display
const interval = setInterval(() => {
  // Game Clock Timer
  if(gameClkEn) {
    gameClkCnt = gameClkCnt - 100; // Decrease countdown by 10 milliseconds
    if (gameClkCnt < 0)  gameClkCnt = 0;
  } 
  gameTimerElement.textContent = formatTime(Math.round(gameClkCnt/1000)); // Update the timer display

  // Shot Clock Timer
  if(shotClkEn) {
    //If the shot clock is greater than the game clock then make them equal
    if(shotClkCnt > gameClkCnt) {
      shotClkCnt = gameClkCnt;
    }
    shotClkCnt = shotClkCnt - 100; // Decrease countdown by 100 milliseconds
    if (shotClkCnt < 0) shotClkCnt = 0;
  } 
  shotClkTimerElement.textContent = Math.round(shotClkCnt/1000); // Update the timer display
  
}, 100); // Update every second

document.addEventListener("DOMContentLoaded", () => {
  document.getElementById('btnRunGameClk').addEventListener('click', toggleRunGameClk);
  document.getElementById('btnRstGameClk').addEventListener('click', rstGameClk);
  document.getElementById('btnEditGameClk').addEventListener('click', editGameClk);
  document.getElementById('btnRunShotClk').addEventListener('click', toggleRunShotClk);
  document.getElementById('btnRstShotClk').addEventListener('click', rstShotClk);
  document.getElementById('btnEditShotClk').addEventListener('click', editShotClk);
  document.getElementById('btnHome').addEventListener('click', () => { location.href = './'; });
  document.getElementById("syncClksCheckbox").addEventListener("change", syncClks);
});

function formatTime(seconds) {
  const minutes = Math.floor(seconds / 60);
  const remainingSeconds = seconds % 60;
  return String(minutes).padStart(2, '0') + ":" + String(remainingSeconds).padStart(2, '0');
}
    
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
  if(rstShotClkCnt > gameClkCnt) shotClkCnt = gameClkCnt;
  else shotClkCnt = rstShotClkCnt;
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

function syncClks() {
  syncGameAndShotClks = this.checked;
  if(syncGameAndShotClks) {
    //If either clock is running, make them both run
    gameClkEn = shotClkEn | gameClkEn;
    shotClkEn = shotClkEn | gameClkEn;
    setGameClkButtonState();
    setShotClkButtonState();
  }
  fetch(syncGameAndShotClks ? '/syncClks' : '/unsyncClks');
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

function isNumber(str) {
    return !isNaN(str) && str.trim() !== ""; // Ensure it's not empty or whitespace
}