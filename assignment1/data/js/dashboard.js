let currentTab = 'day'; // Default to 'day' on page load

document.addEventListener('DOMContentLoaded', async () => {

  const token = localStorage.getItem('jwtToken');
  if (!token) {
    window.location.href = '/login';
    return;
  }

  // Token check
  const response = await fetch('/api/auth-check', {
    headers: { 'Authorization': `Bearer ${token}` }
  });
  if (!response.ok) {
    localStorage.removeItem('jwtToken');
    window.location.href = '/login';
    return;
  }

  // Initialize chart for the 'Day' data by default
  await fetchDataAndUpdate(token, currentTab);
  setInterval(() => fetchDataAndUpdate(token, currentTab), 20000);

  // Set up event listeners for each tab
  document.getElementById('day-tab').addEventListener('click', () => {
    currentTab = 'day';
    fetchDataAndUpdate(token, currentTab);
  });
  document.getElementById('week-tab').addEventListener('click', () => {
    currentTab = 'week';
    fetchDataAndUpdate(token, currentTab);
  });
  document.getElementById('month-tab').addEventListener('click', () => {
    currentTab = 'month';
    fetchDataAndUpdate(token, currentTab);
  });
  document.getElementById('year-tab').addEventListener('click', () => {
    currentTab = 'year';
    fetchDataAndUpdate(token, currentTab);
  });
});

// Function to pad data points for consistent intervals
// Original API data timestamp is in ISO 8601 format
function padDataPoints(data, timestamps, intervalInMs) {
  const now = Date.now();
  const paddedData = [];
  const paddedTimestamps = [];

  //console.log('Interval in ms:', intervalInMs);
  
  if (data.length === 0 || timestamps.length === 0) return { paddedData, paddedTimestamps };

  // Parse the first timestamp to determine the starting point
  let currentTime = Date.parse(timestamps[0].replace(" +00:00", "Z")); // Replace "+00:00" with "Z" to indicate UTC
  //console.log('Curent time raw timestamp:', timestamps[0]);
  //console.log('Current time - first timestamp:', currentTime, new Date(currentTime).toUTCString());

  data.forEach((value, index) => {
    const pointTime = Date.parse(timestamps[index].replace(" +00:00", "Z"));
    //console.log('Current point raw timestamp:', timestamps[index]);
    //console.log('Point time of current point:', pointTime, new Date(pointTime).toUTCString());

    // Insert placeholder data points for any gaps
    //console.log('Fill in missing data points');
    while (currentTime < pointTime) {
      paddedData.push(null); // Null as a placeholder for missing data
      //console.log('Current time:', currentTime, new Date(currentTime).toUTCString());
      const timestamp = new Date(currentTime).toISOString().replace('T', ' ').replace('Z', ' +00:00');
      //console.log('Padded timestamp:', timestamp);
      paddedTimestamps.push(timestamp);
      currentTime += intervalInMs;
      //console.log('Current time after incrementing it by interval in ms:', currentTime, new Date(currentTime).toUTCString());
    }

    // Add the actual data point and timestamp
    paddedData.push(value);
    //console.log('Pushing actual timestamp:', timestamps[index]);
    paddedTimestamps.push(timestamps[index]);
    currentTime += intervalInMs;
    //console.log('Current time after incrementing it by interval in ms:', currentTime, new Date(currentTime).toUTCString());
  });

  // Fill in any remaining points up to the current time
  //console.log('Fill in remaining data points till now. Now is: ', now);
  while (currentTime < now) {
    paddedData.push(null);
    const timestamp = new Date(currentTime).toISOString().replace('T', ' ').replace('Z', ' +00:00');
    paddedTimestamps.push(timestamp);
    //console.log('Padded timestamp:', timestamp);
    currentTime += intervalInMs;
    //console.log('Current time after incrementing it by interval in ms:', currentTime, new Date(currentTime).toUTCString());
  }

  return { paddedData, paddedTimestamps };
}

// Function to fetch data based on the selected time period and update the chart
async function fetchDataAndUpdate(token, period) {
  const response = await fetch(`/api/data?period=${period}`, {
    headers: { 'Authorization': `Bearer ${token}` }
  });

  if (response.ok) {
    const data = await response.json();
    document.getElementById('message').innerText = data.message;

    // Determine the interval based on the period
    let intervalInMs;
    if (period === 'day') {
      intervalInMs = 5 * 60 * 1000; // 5 minutes
    } else if (period === 'week' || period === 'month') {
      intervalInMs = 60 * 60 * 1000; // 1 hour
    } else if (period === 'year') {
      intervalInMs = 6 * 60 * 60 * 1000; // 6 hours
    }

    // Pad data points for consistent intervals
    const { paddedData: paddedTemperatureData, paddedTimestamps } = padDataPoints(data.temperature, data.timestamps, intervalInMs);
    const { paddedData: paddedHumidityData } = padDataPoints(data.humidity, data.timestamps, intervalInMs);

    // Adjust timestamps for display
    const adjustedTimestamps = adjustTimestamps(paddedTimestamps, period);

    updateChart(period, paddedTemperatureData, paddedHumidityData, adjustedTimestamps);
    
    updateActivityLog(data.activityLog);
  } else {
    localStorage.removeItem('jwtToken');
    window.location.href = '/login';
  }
}

// Adjust timestamps based on the selected period and corresponding intervals
function adjustTimestamps(timestamps, period) {
  switch (period) {
    case 'day': // 5-minute intervals
      return timestamps.map(ts => new Date(ts).toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', hour12: true }));
    case 'week': // Hourly intervals
      return timestamps.map(ts => new Date(ts).toLocaleString('en-US', { weekday: 'short', hour: '2-digit', hour12: true }));
    case 'month': // 6-hour intervals
      return timestamps.map(ts => new Date(ts).toLocaleString('en-US', { day: 'numeric', month: 'short', hour: '2-digit', hour12: true }));
    case 'year': // 6-hour intervals
      return timestamps.map(ts => new Date(ts).toLocaleString('en-US', { day: 'numeric', month: 'short', hour: '2-digit', hour12: true }));
    default:
      return timestamps;
  }
}


// Object to hold chart instances for each period
const chartInstances = {};

// Function to initialize a chart for a specific period if it doesn't exist
function setupChart(period, temperatureData, humidityData, timestamps) {
  const ctx = document.getElementById(`tempHumidityChart${capitalize(period)}`).getContext('2d');
  chartInstances[period] = new Chart(ctx, {
    type: 'line',
    data: {
      labels: timestamps,
      datasets: [
        {
          label: 'Temperature (°C)',
          data: temperatureData,
          borderColor: 'rgba(255, 99, 132, 1)',
          backgroundColor: 'rgba(255, 99, 132, 0.1)',
          yAxisID: 'y',
          tension: 0.4,
          pointRadius: 3,
          pointHoverRadius: 6,
          fill: true
        },
        {
          label: 'Humidity (%)',
          data: humidityData,
          borderColor: 'rgba(54, 162, 235, 1)',
          backgroundColor: 'rgba(54, 162, 235, 0.1)',
          yAxisID: 'y1',
          tension: 0.4,
          pointRadius: 3,
          pointHoverRadius: 6,
          fill: true
        }
      ]
    },
    options: {
      maintainAspectRatio: true,
      plugins: {
        legend: { position: 'top' },
        tooltip: { mode: 'index', intersect: false }
      },
      scales: {
        y: {
          type: 'linear',
          position: 'left',
          title: { display: true, text: 'Temperature (°C)' }
        },
        y1: {
          type: 'linear',
          position: 'right',
          title: { display: true, text: 'Humidity (%)' }
        }
      }
    }
  });
}

// Function to update an existing chart instance with new data
function updateChart(period, temperatureData, humidityData, timestamps) {
  if (!chartInstances[period]) {
    setupChart(period, temperatureData, humidityData, timestamps);
    return;
  }

  const chart = chartInstances[period];
  chart.data.labels = timestamps;
  chart.data.datasets[0].data = temperatureData;
  chart.data.datasets[1].data = humidityData;
  chart.update();
}

// Function to update the activity log table
function updateActivityLog(activityLog) {
  if (!activityLog || activityLog.length === 0) {
    document.getElementById('activityLog').innerHTML = '<tr><td colspan="3">No activity to display</td></tr>';
    return;
  }
  const activityLogTable = document.getElementById('activityLog');
  activityLogTable.innerHTML = '';
  activityLog.forEach(logEntry => {
    const row = document.createElement('tr');
    row.innerHTML = `<td>${logEntry.timestamp}</td><td>${logEntry.action}</td><td>${logEntry.details}</td>`;
    activityLogTable.appendChild(row);
  });
}

// Helper function to capitalize the first letter of a string
function capitalize(str) {
  return str.charAt(0).toUpperCase() + str.slice(1);
}