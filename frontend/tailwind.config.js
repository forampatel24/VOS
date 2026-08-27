/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{js,ts,jsx,tsx}"],
  theme: {
    extend: {
      colors: {
        jarvis: {
          bg: "#060a14",
          surface: "#0d1225",
          elevated: "#141b34",
          border: "#1e2a4a",
          cyan: "#00ffff",
          blue: "#0088ff",
          magenta: "#ff00ff",
          dim: "#6b7a9a",
          text: "#e0e8ff",
        },
      },
      fontFamily: {
        mono: ["JetBrains Mono", "Consolas", "monospace"],
        sans: ["Inter", "system-ui", "sans-serif"],
      },
      animation: {
        pulse_slow: "pulse 3s cubic-bezier(0.4, 0, 0.6, 1) infinite",
        glow: "glow 2s ease-in-out infinite alternate",
      },
      keyframes: {
        glow: {
          "0%": { boxShadow: "0 0 10px rgba(0,255,255,0.3)" },
          "100%": { boxShadow: "0 0 20px rgba(0,255,255,0.6)" },
        },
      },
    },
  },
  plugins: [],
};
