function goBack() {
  window.location.href = 'http://ui-action/back';
}

function goForward() {
  window.location.href = 'http://ui-action/forward';
}

function reloadPage() {
  window.location.href = 'http://ui-action/reload';
}

function handleKey(event) {
  if (event.key === 'Enter') {
    let url = event.target.value.trim();
    if (url.length === 0) return;
    
    // Auto prefix protocol if missing (excluding about: and file:)
    if (!/^https?:\/\//i.test(url) && !/^about:/i.test(url) && !/^file:\/\//i.test(url)) {
      // Check if it looks like a search query or a hostname
      if (url.indexOf('.') === -1 || url.indexOf(' ') !== -1) {
        url = 'https://www.google.com/search?q=' + encodeURIComponent(url);
      } else {
        url = 'https://' + url;
      }
    }
    
    window.location.href = 'http://ui-action/load?url=' + encodeURIComponent(url);
  }
}

// Global functions injected by the C++ backend
window.updateNavState = function(canGoBack, canGoForward, isLoading) {
  document.getElementById('back').disabled = !canGoBack;
  document.getElementById('forward').disabled = !canGoForward;
  
  const loader = document.getElementById('loader');
  if (isLoading) {
    loader.classList.remove('hide');
  } else {
    loader.classList.add('hide');
  }
};

window.updateAddress = function(url) {
  // Only update input if it is not currently focused by the user to avoid cursor jumping
  const addressBar = document.getElementById('address-bar');
  if (document.activeElement !== addressBar) {
    // If loading the local UI, show a blank or about:blank address bar
    if (url.indexOf('ui/index.html') !== -1 || url.indexOf('ui-action') !== -1) {
      addressBar.value = '';
    } else {
      addressBar.value = url;
    }
  }
};
