// JavaScript for Pace Clk Page

// let paceClkMilliSecCnt = INITIAL_PACE_CLK_VAL; // Set the countdown start value in milliseconds
// let paceClkEn = PACE_CLK_ENABLE; // Set the initial enable state
// let paceClkRolloverMilliSecCnt = PACE_CLK_ROLLOVER_VAL; // Set the Reset state
// let paceCountdown = INITIAL_PACE_COUNTDOWN_STATE;
let paceClkMilliSecCnt = 0; // Set the countdown start value in milliseconds
let paceClkEn = true; // Set the initial enable state
let paceClkRolloverMilliSecCnt = 60; // Set the Reset state
const paceClkTimerElement = document.getElementById('paceClkTimer');
let paceCountdown = false;
let paceClkCnt = 0;
let intervalCount = 3;

getPaceCountdown();

//Create internal timers to update the display
const interval = setInterval(() => {
  // Pace Clock Timer
  if(paceClkEn) { 
    if(paceCountdown) paceClkMilliSecCnt = paceClkMilliSecCnt - 100; // Decrease countdown by 100 milliseconds
    else paceClkMilliSecCnt = paceClkMilliSecCnt + 100; // Increase countdown by 100 milliseconds

    paceClkCnt = Math.round(paceClkMilliSecCnt/1000);
    if(paceCountdown & (paceClkCnt <= 0)) {
      paceClkMilliSecCnt = paceClkMilliSecCnt + paceClkRolloverMilliSecCnt; //Rollover for countdown
      paceClkCnt = paceClkRolloverMilliSecCnt/1000;
    } else if(!paceCountdown & (paceClkCnt >= paceClkRolloverMilliSecCnt/1000)) { 
      paceClkMilliSecCnt = paceClkMilliSecCnt - paceClkRolloverMilliSecCnt; //Rollover for count up
      paceClkCnt = 0;
    }
  }
  paceClkTimerElement.textContent = paceClkCnt; // Update the timer 
}, 100); // Update every second

document.addEventListener("DOMContentLoaded", () => {
  document.getElementById('btnHome').addEventListener('click', () => { location.href = './'; });
  document.getElementById('btnRunPaceClk').addEventListener('click', toggleRunPaceClk);
  document.getElementById('btnRstPaceClk').addEventListener('click', rstPaceClk);
  document.getElementById('btnEditPaceClk').addEventListener('click', editPaceClk);
  document.getElementById('checkboxPaceCountdown').addEventListener('change', getPaceCountdown);
  document.getElementById('txtboxRolloverVal').addEventListener('change', setPaceRolloverVal);
});

function toggleRunPaceClk() {
  paceClkEn = !paceClkEn; 
  setPaceClkButtonState();
  fetch('/toggleRunPaceClk');
}

function rstPaceClk() {
  //If the pace clock is counting down then reset to th rollover time.
  //If the pace clock is counting up then reset to zero.
  paceClkMilliSecCnt = paceCountdown ? paceClkRolloverMilliSecCnt : 0;
  paceClkTimerElement.textContent = paceClkCnt; // Update the timer 
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
      paceClkMilliSecCnt = totalMilliseconds;
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

function getPaceCountdown() {
  paceCountdown = document.getElementById("checkboxPaceCountdown").checked;
  // fetch('/togglePaceCountdown');
  fetch(`/togglePaceCountdown?paceCountdown=${paceCountdown}`);
}

function setPaceRolloverVal() {
  let paceClkRolloverCnt = document.getElementById("txtboxRolloverVal").value;
  paceClkRolloverMilliSecCnt = 1000*paceClkRolloverCnt;
  fetch(`/setPaceRolloverVal?paceClkRolloverCnt=${paceClkRolloverCnt}`);
}

function addInterval() {
  intervalCount++;
  const container = document.getElementById("intervals");
  const row = document.createElement("div");
  row.className = "interval-row";
  row.innerHTML = `
    <input type="text" value="Interval ${intervalCount}" class="label-input" />
    <input type="number" value="45" min="1" class="time-input" />
    <button onclick="removeInterval(this)">Remove</button>
  `;
  container.appendChild(row);
}

function removeInterval(button) {
  const row = button.parentElement;
  row.remove();
}

function startSequence() {
  currentIndex = 0;
  highlightInterval(currentIndex);
  // You could start a countdown here
}

function resetSequence() {
  currentIndex = 0;
  highlightInterval(-1); // remove all highlights
}

function highlightInterval(index) {
  const rows = document.querySelectorAll(".interval-row");
  rows.forEach((row, i) => {
    row.classList.toggle("active", i === index);
  });
}

function nextInterval() {
  const rows = document.querySelectorAll(".interval-row");
  if (currentIndex < rows.length - 1) {
    currentIndex++;
  } else {
    currentIndex = 0;
  }
  highlightInterval(currentIndex);
}


document.addEventListener('DOMContentLoaded', () => {
  const countdownEl = document.getElementById('paceClkTimer');

  // helper to format MM:SS
  function formatTime(sec) {
    const m = Math.floor(sec/60).toString().padStart(2,'0');
    const s = (sec%60).toString().padStart(2,'0');
    return `${m}:${s}`;
  }

  // updated countdown() that drives the bottom bar
  function countdown(seconds) {
    return new Promise(resolve => {
      let remaining = seconds;
      countdownEl.textContent = formatTime(remaining);
      const id = setInterval(() => {
        remaining--;
        countdownEl.textContent = formatTime(remaining);
        if (remaining <= 0) {
          clearInterval(id);
          resolve();
        }
      }, 1000);
    });
  }

  // integrate into your interval sequence
  async function runIntervalSequence() {
    const rows = Array.from(document.querySelectorAll('.interval-row'));
    for (let i = 0; i < rows.length; i++) {
      highlightInterval(i);
      const secs = +rows[i].querySelector('.time-input').value;
      await countdown(secs);
    }
    resetSequence();
  }

  // wire your Start button to runIntervalSequence()
  document.querySelector('button[onclick="startSequence()"]')
    .addEventListener('click', () => {
      currentIndex = 0;
      runIntervalSequence();
    });
});
