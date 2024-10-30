document.getElementById('loginForm').addEventListener('submit', async (e) => {
  e.preventDefault();
  
  const username = document.getElementById('username').value;
  const password = document.getElementById('password').value;
  const response = await fetch('/login', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: new URLSearchParams({ username, password })
  });

  const data = await response.json();
  if (response.ok) {
    localStorage.setItem('jwtToken', data.token);
    window.location.href = '/dashboard.html';
  } else {
    document.getElementById('loginError').style.display = 'block';
  }
});
