document.addEventListener('DOMContentLoaded', async () => {
  const token = localStorage.getItem('jwtToken');
  if (!token) {
    window.location.href = '/login';
    return;
  }

  // Token check
  const authCheckResponse = await fetch('/api/auth-check', {
    headers: { 'Authorization': `Bearer ${token}` }
  });
  if (!authCheckResponse.ok) {
    localStorage.removeItem('jwtToken');
    window.location.href = '/login';
    return;
  }

  // Load config data on page load
  await loadConfigData(token);

  // Handle form submission
  document.getElementById('settingsForm').addEventListener('submit', async (event) => {
    event.preventDefault(); // Prevent form from submitting the traditional way
    await saveConfigData(token);
  });
});

async function loadConfigData(token) {
  try {
    const response = await fetch('/api/config', {
      headers: { 'Authorization': `Bearer ${token}` }
    });

    if (!response.ok) {
      throw new Error('Failed to fetch config data');
    }

    const config = await response.json();

    // Populate form fields with the config data
    document.getElementById('wifi-ssid').value = config.wifi.ssid || '';
    document.getElementById('wifi-password').value = config.wifi.password || '';
    document.getElementById('wifi-username').value = config.wifi.username || '';
    document.getElementById('wifi-security').value = config.wifi.security || '';
    document.getElementById('wifi-identity').value = config.wifi.identity || '';
    document.getElementById('login-user').value = config.login.user || '';
    document.getElementById('login-password').value = config.login.password || '';
    document.getElementById('jwt-secret').value = config.jwtSecret || '';
    document.getElementById('timezone').value = config.timezone || '';
  } catch (error) {
    console.error(error);
    alert("Failed to load settings. Please try again.");
  }
}

async function saveConfigData(token) {
  const configData = {
    wifi: {
      ssid: document.getElementById('wifi-ssid').value,
      password: document.getElementById('wifi-password').value,
      username: document.getElementById('wifi-username').value,
      security: document.getElementById('wifi-security').value,
      identity: document.getElementById('wifi-identity').value
    },
    login: {
      user: document.getElementById('login-user').value,
      password: document.getElementById('login-password').value
    },
    jwtSecret: document.getElementById('jwt-secret').value,
    timezone: document.getElementById('timezone').value
  };

  try {
    const response = await fetch('/api/config', {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${token}`,
        'Content-Type': 'application/x-www-form-urlencoded'
      },
      body: 'config=' + encodeURIComponent(JSON.stringify(configData)) //https://stackoverflow.com/a/42312303 - webserver expects config param.
    });

    if (!response.ok) {
      throw new Error('Failed to save config data');
    }

    alert("Settings saved successfully!");
  } catch (error) {
    console.error(error);
    alert("Failed to save settings. Please try again.");
  }
}