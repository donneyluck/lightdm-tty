// bridge.js — compatibility shim for web-greeter
// web-greeter injects window.lightdm natively; this file is a safe no-op in that case.
(function() {
    if (typeof window.lightdm !== 'undefined') return;

    // Fallback: if no greeter injected lightdm, provide a minimal stub
    // so the page doesn't crash entirely (useful for debugging in a plain browser).
    // For real browser testing, use mock.js instead.
    console.warn('lightdm not found — greeter may not have injected it yet.');
})();
