"use strict";

const form = document.querySelector("#request-form");
const send = document.querySelector("#send");
const feedback = document.querySelector("#feedback");
const status = document.querySelector("#status");
const headers = document.querySelector("#headers");
const body = document.querySelector("#body");

form.addEventListener("submit", async (event) => {
  event.preventDefault();
  const method = document.querySelector("#method").value;
  const path = document.querySelector("#path").value;
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 10000);
  send.disabled = true;
  status.textContent = "Waiting";
  delete status.dataset.kind;
  headers.textContent = "Waiting for response headers…";
  body.textContent = "Waiting for a response…";
  feedback.textContent = `Sending ${method} ${path}…`;

  try {
    const response = await fetch(path, {
      method,
      cache: "no-store",
      mode: "same-origin",
      signal: controller.signal,
    });
    const text = await response.text();
    status.textContent = `${response.status} ${response.statusText}`.trim();
    status.dataset.kind = response.ok ? "success" : "error";
    headers.textContent = Array.from(response.headers, ([key, value]) => `${key}: ${value}`).join("\n");
    body.textContent = method === "HEAD" ? "No response body: this was a HEAD request." : text || "(empty body)";
    feedback.textContent = `Received ${response.status} for ${method} ${path}.`;
  } catch (error) {
    status.textContent = "Request failed";
    status.dataset.kind = "error";
    headers.textContent = "No complete response received.";
    body.textContent = "Open this page through Tez at /static/index.html, and check that the server is running with the example routes.";
    feedback.textContent = error.name === "AbortError"
      ? "The request did not finish within 10 seconds. Check the server, then retry."
      : "The request could not be completed. Check the server, then retry.";
  } finally {
    clearTimeout(timeout);
    send.disabled = false;
  }
});
