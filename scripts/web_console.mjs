// Opens a page in headless Chrome for a while and prints what it wrote to
// the console, one line per message ("log: ...", "error: ...", and
// "exception: ..." for an uncaught error), then saves a screenshot.
// scripts/web_game_test.sh uses it to check the web export starts.
//
//   node scripts/web_console.mjs <chrome> <url> <seconds> <screenshot.png>
//
// Talks to Chrome over its DevTools protocol with Node's own WebSocket
// (Node 22 or later), so it needs no packages.

import { spawn } from 'node:child_process';
import { mkdtempSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const [chromePath, url, secondsText, screenshotPath] = process.argv.slice(2);
if (!screenshotPath) {
    console.error('usage: node web_console.mjs <chrome> <url> <seconds> <screenshot.png>');
    process.exit(2);
}
const seconds = Number(secondsText);

// No GPU on CI's runners: WebGL 2 through SwiftShader, Chrome's software
// renderer. A fresh profile, so no earlier run's cache or service worker.
const chrome = spawn(chromePath, [
    '--headless=new',
    '--no-sandbox',
    '--no-first-run',
    '--disable-background-networking',
    '--use-angle=swiftshader',
    '--enable-unsafe-swiftshader',
    '--window-size=1280,720',
    `--user-data-dir=${mkdtempSync(join(tmpdir(), 'web-console-'))}`,
    '--remote-debugging-port=0',
    'about:blank',
], { stdio: ['ignore', 'ignore', 'pipe'] });

// Chrome prints its DevTools address on stderr once it listens.
const browserUrl = await new Promise((resolve, reject) => {
    let text = '';
    chrome.stderr.on('data', (chunk) => {
        text += chunk;
        const match = text.match(/DevTools listening on (ws:\/\/\S+)/);
        if (match) {
            resolve(match[1]);
        }
    });
    chrome.on('exit', (code) => reject(new Error(`Chrome exited (${code}) before listening`)));
});

const socket = new WebSocket(browserUrl);
await new Promise((resolve, reject) => {
    socket.onopen = resolve;
    socket.onerror = reject;
});

let nextId = 1;
const pending = new Map();
const lines = [];
socket.onmessage = (event) => {
    const message = JSON.parse(event.data);
    if (message.id !== undefined) {
        const callback = pending.get(message.id);
        pending.delete(message.id);
        if (message.error) {
            callback.reject(new Error(message.error.message));
        } else {
            callback.resolve(message.result);
        }
        return;
    }
    const params = message.params;
    if (message.method === 'Runtime.consoleAPICalled') {
        const text = params.args.map((arg) => arg.value ?? arg.description ?? '').join(' ');
        lines.push(`${params.type}: ${text}`);
    } else if (message.method === 'Runtime.exceptionThrown') {
        const details = params.exceptionDetails;
        lines.push(`exception: ${details.exception?.description ?? details.text}`);
    } else if (message.method === 'Log.entryAdded' && params.entry.level === 'error') {
        // The browser's own errors, e.g. a file that failed to load.
        lines.push(`error: ${params.entry.text} ${params.entry.url ?? ''}`.trimEnd());
    }
};

function send(method, params = {}, sessionId = undefined) {
    const id = nextId++;
    socket.send(JSON.stringify({ id, method, params, sessionId }));
    return new Promise((resolve, reject) => pending.set(id, { resolve, reject }));
}

const { targetId } = await send('Target.createTarget', { url: 'about:blank' });
const { sessionId } = await send('Target.attachToTarget', { targetId, flatten: true });
await send('Runtime.enable', {}, sessionId);
await send('Log.enable', {}, sessionId);
await send('Page.enable', {}, sessionId);
await send('Page.navigate', { url }, sessionId);
await new Promise((resolve) => setTimeout(resolve, seconds * 1000));

const shot = await send('Page.captureScreenshot', { format: 'png' }, sessionId);
writeFileSync(screenshotPath, Buffer.from(shot.data, 'base64'));
console.log(lines.join('\n'));
chrome.kill();
process.exit(0);
