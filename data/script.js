var button = document.querySelectorAll('.button')[0];
button.addEventListener('click', function () {
  var xhr = new XMLHttpRequest();
  xhr.open('POST', '/wakeup?wakeup=1');
  xhr.onload = function() {
    if (xhr.status === 200) {
      console.log('Success!');
    }
    else {
      alert("Failed ;(");
    }
  };
  xhr.onerror = function() {
    alert("Failed ;(");
  };
  xhr.send();
});