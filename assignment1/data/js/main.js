// main.js

document.addEventListener('DOMContentLoaded', async () => {
  // Token check
  const token = localStorage.getItem('jwtToken');
  if (!token) {
    window.location.href = '/login';
    return;
  }

  // Verify the token with the server
  const response = await fetch('/api/auth-check', {
    headers: { 'Authorization': `Bearer ${token}` }
  });
  if (!response.ok) {
    localStorage.removeItem('jwtToken');
    window.location.href = '/login';
    return;
  }

  // Highlight the active link based on the current path
  const path = window.location.pathname;
  const navLinks = {
    "" : document.getElementById("dashboardLink"),
    "/" : document.getElementById("dashboardLink"),
    "/dashboard": document.getElementById("dashboardLink"),
    "/settings": document.getElementById("settingsLink"),
    "/rules": document.getElementById("rulesLink")
  };
  for (const link in navLinks) {
    navLinks[link]?.classList.remove("active");
  }
  if (navLinks[path]) {
    navLinks[path].classList.add("active");
  }

  document.getElementById('logoutButton').addEventListener('click', () => {
    localStorage.removeItem('jwtToken');
    window.location.href = '/login';
  });
});
