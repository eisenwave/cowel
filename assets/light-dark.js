const theme = localStorage.getItem("mmml-theme");
if (theme !== null) {
    document.documentElement.className = theme;
}
