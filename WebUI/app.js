window.addEventListener('DOMContentLoaded', function () {
  // History buffers for each PID channel (trend chart)
  var hist = { ch1: [], ch2: [], ch3: [], ch4: [] };
  var maxPoints = 200;
  var pollInterval = 1000;

  // ==================== API Fetch ====================
  function fetchData() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', '/api/data', true);
    xhr.onload = function () {
      if (xhr.status === 200) {
        try {
          var data = JSON.parse(xhr.responseText);
          renderSensors(data.sensors);
          renderPID(data.pid_channels);
          renderOutputs(data.outputs);
          renderSystem(data.system, data.uptime_ms);
          updateTrend(data.pid_channels);
        } catch (e) { console.error(e); }
      }
    };
    xhr.onerror = function () { console.error('XHR error'); };
    xhr.send();
  }

  // ==================== Render Sensors ====================
  function renderSensors(sensors) {
    var grid = document.getElementById('sensor-grid');
    if (!grid) return;
    grid.innerHTML = '';

    sensors.forEach(function (s) {
      var card = document.createElement('div');
      card.className = 'sensor-card';

      var faultClass = s.fault === 0 ? 'fault-ok' : 'fault-bad';
      var tempClass = s.valid ? 'temp-value' : 'temp-value fault';

      card.innerHTML =
        '<h3><span class="fault-indicator ' + faultClass + '"></span>Sensor ' + s.ch + '</h3>' +
        '<div class="' + tempClass + '">' + s.temp.toFixed(1) + '\u00B0C</div>' +
        '<div style="font-size:11px;color:#8b949e">' +
          'CJ: ' + s.cj.toFixed(1) + '\u00B0C | Fault: ' + s.fault +
        '</div>';

      grid.appendChild(card);
    });
  }

  // ==================== Render PID Channels ====================
  function renderPID(channels) {
    var grid = document.getElementById('pid-grid');
    if (!grid) return;
    grid.innerHTML = '';

    channels.forEach(function (p) {
      var card = document.createElement('div');
      card.className = 'pid-card';
      card.id = 'pid-card-' + p.ch;

      card.innerHTML =
        '<h3>PID Channel ' + p.ch + ' (Sensor ' + p.sensor_idx + ')</h3>' +
        '<div>SP: <input type="number" value="' + p.sp.toFixed(1) +
          '" onchange="window.updateSP(' + p.ch + ', this.value)" step="0.5"> \u00B0C</div>' +
        '<div class="output-bar"><div class="output-bar-fill" style="width:' + p.output + '%"></div></div>' +
        '<div style="font-size:13px">Output: ' + p.output.toFixed(1) + '% | Mode: ' + p.mode + '</div>' +
        '<div class="tuning-info">Kp=' + p.kp.toFixed(4) +
          ' Ki=' + p.ki.toFixed(4) +
          ' Kd=' + p.kd.toFixed(4) + '</div>' +
        '<button class="' + (p.mode === 'auto' ? 'active' : '') +
          '" onclick="window.toggleMode(' + p.ch + ')">' +
          p.mode.toUpperCase() + '</button>' +
        '<button onclick="window.autoTune(' + p.ch + ', 1)">Auto-Tune Z-N</button>' +
        '<button onclick="window.autoTune(' + p.ch + ', 2)">Auto-Tune C-C</button>';

      grid.appendChild(card);
    });
  }

  // ==================== Render Output Channels ====================
  function renderOutputs(outputs) {
    var grid = document.getElementById('output-grid');
    if (!grid) return;
    grid.innerHTML = '';

    outputs.forEach(function (o) {
      var card = document.createElement('div');
      card.className = 'output-card';

      card.innerHTML =
        '<h3>Output ' + o.ch + '</h3>' +
        '<div class="output-bar"><div class="output-bar-fill" style="width:' + o.duty + '%"></div></div>' +
        '<div>Duty: ' + o.duty.toFixed(1) + '% | ' + (o.enabled ? 'ENABLED' : 'DISABLED') + '</div>';

      grid.appendChild(card);
    });
  }

  // ==================== Render System Status ====================
  function renderSystem(sys, uptime) {
    var stateEl = document.getElementById('sys-state');
    var states = ['INIT', 'IDLE', 'RUNNING', 'FAULT', 'EMERGENCY'];
    stateEl.textContent = 'State: ' + (states[sys.state] || 'UNKNOWN');
    stateEl.style.color = sys.emergency ? '#f85149' : '#c9d1d9';

    var alarmEl = document.getElementById('sys-alarms');
    alarmEl.textContent = 'Alarms: ' + sys.alarms;
    alarmEl.className = sys.alarms > 0 ? 'alarm-on' : 'alarm-off';

    var uptimeEl = document.getElementById('sys-uptime');
    uptimeEl.textContent = 'Uptime: ' + (uptime / 1000).toFixed(1) + 's';
  }

  // ==================== Trend Chart (Canvas) ====================
  function updateTrend(channels) {
    channels.forEach(function (p) {
      var h = hist['ch' + p.ch];
      if (!h) return;
      h.push(p.sp);
      if (h.length > maxPoints) h.shift();
    });
    drawChart();
  }

  function drawChart() {
    var canvas = document.getElementById('trend-chart');
    if (!canvas) return;
    var ctx = canvas.getContext('2d');
    var w = canvas.width;
    var h = canvas.height;

    // Background
    ctx.fillStyle = '#0d1117';
    ctx.fillRect(0, 0, w, h);

    // Grid lines
    ctx.strokeStyle = '#30363d';
    ctx.lineWidth = 1;
    for (var i = 0; i < 5; i++) {
      var y = (h / 5) * i;
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(w, y);
      ctx.stroke();
    }

    // Draw trend lines for 4 PID channels
    var colors = ['#58a6ff', '#3fb950', '#d2991d', '#f85149'];
    for (var ch = 1; ch <= 4; ch++) {
      var data = hist['ch' + ch] || [];
      if (data.length < 2) continue;

      ctx.strokeStyle = colors[ch - 1];
      ctx.lineWidth = 2;
      ctx.beginPath();
      for (var i = 0; i < data.length; i++) {
        var x = (i / data.length) * w;
        var yVal = h - ((data[i] / 450) * h);
        if (i === 0) { ctx.moveTo(x, yVal); }
        else         { ctx.lineTo(x, yVal); }
      }
      ctx.stroke();
    }

    // Labels
    ctx.fillStyle = '#8b949e';
    ctx.font = '10px monospace';
    ctx.fillText('0\u00B0C', 2, h - 4);
    ctx.fillText('450\u00B0C', 2, 12);
  }

  // ==================== User Actions ====================
  window.toggleMode = function (ch) {
    var xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/control?ch=' + ch + '&action=toggle_mode', true);
    xhr.send();
  };

  window.updateSP = function (ch, val) {
    var xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/control?ch=' + ch + '&action=setpoint&value=' + parseFloat(val), true);
    xhr.send();
  };

  window.autoTune = function (ch, method) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', '/api/autotune?ch=' + ch + '&cmd=' + method, true);
    xhr.onload = function () {
      try {
        var r = JSON.parse(xhr.responseText);
        console.log('Auto-tune response:', r);
      } catch (e) { }
    };
    xhr.send();
  };

  // ==================== Start Polling ====================
  fetchData();
  setInterval(fetchData, pollInterval);
});
