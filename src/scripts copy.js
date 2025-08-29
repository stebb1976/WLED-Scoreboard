
let gameClkCnt = INITIAL_GAME_CLK_VAL; // Set the countdown start value in milliseconds
let gameClkEn = GAME_CLK_ENABLE; // Set the initial enable state
let rstGameClkCnt = RESET_GAME_CLK_VAL; // Set the Reset state
const gameClkTimerElement = document.getElementById('gameClkTimer');
let shotClkCnt = INITIAL_SHOT_CLK_VAL; // Set the countdown start value in seconds
let shotClkEn = SHOT_CLK_ENABLE; // Set the initial enable state
let rstShotClkCnt = RESET_SHOT_CLK_VAL; // Set the Reset state
const shotClkTimerElement = document.getElementById('shotClkTimer');
let syncGameAndShotClks = SYNC_CLKS; // Set the initial sync state 

document.getElementById("syncClksCheckbox").checked = INITIAL_CHECKBOX_STATE; 

const interval = setInterval(() => {
  // Game Clock Timer
  gameClkTimerElement.textContent = formatTime(Math.round(gameClkCnt/1000)); // Update the timer display
  if(gameClkEn) {gameClkCnt = gameClkCnt - 100; // Decrease countdown by 100 milliseconds
    if (gameClkCnt < 0) {
      clearInterval(gameInterval); // Stop the timer when it reaches 0
    }
  }

  // Shot Clock Timer
  shotClkTimerElement.textContent = Math.round(shotClkCnt/1000); // Update the timer display
  if(shotClkEn) {shotClkCnt = shotClkCnt - 100; // Decrease countdown by 100 milliseconds
    if (shotClkCnt < 0) {
      clearInterval(interval); // Stop the timer when it reaches 0
    }
  } 
}, 100); // Update every second

// Event listener for the checkbox
document.getElementById("syncClksCheckbox").addEventListener("change", function() {
  if (this.checked) {
    syncGameAndShotClks = true;
    fetch('/syncClks').then(() => location.reload());
  } else {
    syncGameAndShotClks = false;
    fetch('/unsyncClks').then(() => location.reload());
  }
});

function formatTime(seconds) {
  const minutes = Math.floor(seconds / 60);
  const remainingSeconds = seconds % 60;
  return String(minutes).padStart(2, '0') + ":" + String(remainingSeconds).padStart(2, '0');
}

function toggleRunGameClk() {
  gameClkEn = !gameClkEn;

  // Update the button's appearance immediately without reloading
  const btn = document.getElementById('btnRunGameClk');
  if (gameClkEn) {
    btn.style.backgroundColor = '#c9535a';
    btn.textContent = 'Stop';
  } else {
    btn.style.backgroundColor = '#63de3e';
    btn.textContent = 'Run';
  }

  if (syncGameAndShotClks) {
    shotClkEn = gameClkEn;
    fetch('/toggleRunShotClk');
  }
  fetch('/toggleRunGameClk');
}

function rstGameClk() {
  gameClkCnt = rstGameClkCnt;
  fetch('/rstGameClk').then(() => location.reload());
}

function editGameClk() {
  let timeStr = prompt("Enter Game Time in m:ss");
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
        fetch(`/editGameClk?time=${totalMilliseconds}`).then(() => location.reload());
      } else {
        alert("Invalid time. Please enter a valid time in m::ss format.");
      }
    } else {
      alert("Invalid format. Please use m::ss format.");
    }
  }
}

function toggleRunShotClk() {
  // $('#btnRunShotClk').click(function(e){e.preventDefault();}).click();
  shotClkEn = !shotClkEn; 
  if (syncGameAndShotClks) {
    gameClkEn = shotClkEn;
    fetch('/toggleRunGameClk');
  }
  fetch('/toggleRunShotClk').then(() => location.reload());
}

function rstShotClk() {
  shotClkCnt = rstShotClkCnt;
  fetch('/rstShotClk').then(() => location.reload());
}