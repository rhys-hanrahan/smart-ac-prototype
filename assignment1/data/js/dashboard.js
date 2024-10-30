// dashboard.js

document.addEventListener('DOMContentLoaded', async () => {
  const token = localStorage.getItem('jwtToken');
  if (!token) {
    window.location.href = '/login';
    return;
  }
  
  // Fetch and update chart data and activity log periodically
  await fetchDataAndUpdate(token);
  setInterval(() => fetchDataAndUpdate(token), 20000);

  // Logout button handler
  document.getElementById('logoutButton').addEventListener('click', () => {
    localStorage.removeItem('jwtToken');
    window.location.href = '/login';
  });
});

async function fetchDataAndUpdate(token) {
  const response = await fetch('/api/data', {
    headers: { 'Authorization': `Bearer ${token}` }
  });

  if (response.ok) {
    const data = await response.json();
    document.getElementById('message').innerText = data.message;
    updateChart(data.temperature, data.humidity, data.timestamps);
    updateActivityLog(data.activityLog);
  } else {
    localStorage.removeItem('jwtToken');
    window.location.href = '/login';
  }
}

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
        legend: {
          position: 'top',
          labels: {
            font: { size: 14 }
          }
        },
        tooltip: {
          mode: 'index',
          intersect: false
        }
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
        },
        x: {
          ticks: { autoSkip: true, maxTicksLimit: 10 }
        }
      }
    }
  });
}

function updateChart(temperatureData, humidityData, timestamps) {
  if (!tempHumidityChart) {
    setupChart(temperatureData, humidityData, timestamps);
    return;
  }
  tempHumidityChart.data.labels = timestamps;
  tempHumidityChart.data.datasets[0].data = temperatureData;
  tempHumidityChart.data.datasets[1].data = humidityData;
  tempHumidityChart.update();
}

function updateActivityLog(activityLog) {
  const activityLogTable = document.getElementById('activityLog');
  activityLogTable.innerHTML = '';
  activityLog.forEach(logEntry => {
    const row = document.createElement('tr');
    row.innerHTML = `<td>${logEntry.timestamp}</td><td>${logEntry.action}</td><td>${logEntry.details}</td>`;
    activityLogTable.appendChild(row);
  });
}
