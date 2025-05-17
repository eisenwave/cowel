const theme = localStorage.getItem("cowel-theme");
if (theme !== null) {
    document.documentElement.className = theme;
}
