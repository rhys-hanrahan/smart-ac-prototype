document.addEventListener('DOMContentLoaded', async () => {
  const token = localStorage.getItem('jwtToken');
  if (!token) {
    window.location.href = '/login';
    return;
  }

  // Initial data fetch and setup
  await fetchDataAndUpdate(token);

  // Polling every 30 seconds to fetch and update data
  setInterval(() => fetchDataAndUpdate(token), 30000);

  // Logout button handler
  document.getElementById('logoutButton').addEventListener('click', () => {
    localStorage.removeItem('jwtToken');
    window.location.href = '/login';
  });
});

// Fetch data from the server and update chart and activity log
async function fetchDataAndUpdate(token) {
  const response = await fetch('/api/data', {
    headers: { 'Authorization': `Bearer ${token}` }
  });

  if (response.ok) {
    const data = await response.json();

    // Update the welcome message
    document.getElementById('message').innerText = data.message;

    // Update the chart with new data
    updateChart(data.temperature, data.humidity, data.timestamps);

    // Update the activity log with new entries
    updateActivityLog(data.activityLog);
  } else {
    alert("Session expired or access denied. Redirecting to login.");
    localStorage.removeItem('jwtToken');
    window.location.href = '/login';
  }
}

// Function to initialize and update Chart.js chart with new data
let tempHumidityChart;

function setupChart(temperatureData, humidityData, timestamps) {
  const ctx = document.getElementById('tempHumidityChart').getContext('2d');
  tempHumidityChart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: timestamps,
      datasets: [
        {
          label: 'Temperature (°C)',
          data: temperatureData,
          borderColor: 'rgba(255, 99, 132, 1)',
          backgroundColor: 'rgba(255, 99, 132, 0.2)',
          yAxisID: 'y'
        },
        {
          label: 'Humidity (%)',
          data: humidityData,
          borderColor: 'rgba(54, 162, 235, 1)',
          backgroundColor: 'rgba(54, 162, 235, 0.2)',
          yAxisID: 'y1'
        }
      ]
    },
    options: {
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

function updateChart(temperatureData, humidityData, timestamps) {
  // Check if chart has been initialized; if not, set it up
  if (!tempHumidityChart) {
    setupChart(temperatureData, humidityData, timestamps);
    return;
  }

  // Update chart data
  tempHumidityChart.data.labels = timestamps;
  tempHumidityChart.data.datasets[0].data = temperatureData;
  tempHumidityChart.data.datasets[1].data = humidityData;
  tempHumidityChart.update();
}

// Function to populate and update activity log table
function populateActivityLog(activityLog) {
  const activityLogTable = document.getElementById('activityLog');
  activityLog.forEach(logEntry => {
    const row = document.createElement('tr');
    row.innerHTML = `
      <td>${logEntry.timestamp}</td>
      <td>${logEntry.action}</td>
      <td>${logEntry.details}</td>
    `;
    activityLogTable.appendChild(row);
  });
}

function updateActivityLog(activityLog) {
  // Clear existing log entries
  const activityLogTable = document.getElementById('activityLog');
  activityLogTable.innerHTML = '';

  // Repopulate with new log entries
  populateActivityLog(activityLog);
}
