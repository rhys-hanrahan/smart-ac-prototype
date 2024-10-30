document.addEventListener('DOMContentLoaded', async () => {
  const token = localStorage.getItem('jwtToken');
  if (!token) {
    window.location.href = '/index.html';
    return;
  }

  const response = await fetch('/protected', {
    headers: { 'Authorization': `Bearer ${token}` }
  });

  if (response.ok) {
    const data = await response.json();
    document.getElementById('message').innerText = data.message;
  } else {
    alert("Access denied. Redirecting to login.");
    localStorage.removeItem('jwtToken');
    window.location.href = '/index.html';
  }
});

document.getElementById('logoutButton').addEventListener('click', () => {
  localStorage.removeItem('jwtToken');
  window.location.href = '/index.html';
});

