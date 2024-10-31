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
    document.getElementById('login-user').value = config.login.user || '';
    document.getElementById('login-password').value = config.login.password || '';
    document.getElementById('thingspeak-api-key').value = config.thingspeak.api_key || '';
    document.getElementById('thingspeak-channel-id').value = config.thingspeak.channel_id || '';
  } catch (error) {
    console.error(error);
    alert("Failed to load settings. Please try again.");
  }
}

async function saveConfigData(token) {
  const configData = {
    wifi: {
      ssid: document.getElementById('wifi-ssid').value,
      password: document.getElementById('wifi-password').value
    },
    login: {
      user: document.getElementById('login-user').value,
      password: document.getElementById('login-password').value
    },
    thingspeak: {
      api_key: document.getElementById('thingspeak-api-key').value,
      channel_id: parseInt(document.getElementById('thingspeak-channel-id').value, 10)
    }
  };

  try {
    const response = await fetch('/api/config', {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${token}`,
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(configData)
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