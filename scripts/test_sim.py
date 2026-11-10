#!/usr/bin/env python3
"""Static and simulated runtime verification of web/sim.html."""
import re
import subprocess
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SIM_HTML = REPO_ROOT / "web" / "sim.html"


def main():
    if not SIM_HTML.is_file():
        raise FileNotFoundError(f"Missing simulator source: {SIM_HTML}")

    html = SIM_HTML.read_text(encoding="utf-8")
    html_ids = set(re.findall(r'\bid="([^"]+)"', html))
    for m in range(48, 75):
        html_ids.add(f"key-{m}")
    for idx in range(16):
        html_ids.add(f"step-led-{idx}")

    script_m = re.search(r"<script>\n(.*?)\n</script>", html, re.S)
    assert script_m, "<script> tag missing from web/sim.html"
    js = script_m.group(1)

    js_ids = set(re.findall(r"document\.getElementById\(['\"]([^'\"]+)['\"]\)", js))
    missing_ids = sorted(i for i in js_ids if i not in html_ids)
    assert not missing_ids, f"Missing DOM IDs referenced in JS: {missing_ids}"

    data_phrases_html = set(re.findall(r'data-phrase="([^"]+)"', html))

    with tempfile.NamedTemporaryFile("w", suffix=".js", delete=False, encoding="utf-8") as tf:
        tf.write("""
const noop = () => {};
const fakeEl = () => ({
  textContent: '', innerHTML: '', className: '',
  style: {}, dataset: {},
  classList: { add: noop, remove: noop, toggle: noop },
  addEventListener: noop, appendChild: noop
});
const canvasCtx = new Proxy({
  clearRect: noop, fillRect: noop, strokeRect: noop, beginPath: noop,
  moveTo: noop, lineTo: noop, quadraticCurveTo: noop, bezierCurveTo: noop,
  arc: noop, ellipse: noop, stroke: noop, fill: noop, closePath: noop,
  fillText: noop, save: noop, restore: noop, setLineDash: noop
}, {
  get(t, p) { return p in t ? t[p] : noop; }
});
global.setInterval = () => 1;
global.clearInterval = noop;
global.document = {
  getElementById: (id) => id === 'lcd' ? { getContext: () => canvasCtx } : fakeEl(),
  querySelectorAll: () => [],
  createElement: () => fakeEl()
};
class MockAudioContext {
  constructor(opts = {}) { this.sampleRate = opts.sampleRate || 44100; this.destination = {}; this.state = 'running'; }
  resume() { return Promise.resolve(); }
  createScriptProcessor(sz) { return { onaudioprocess: null, connect: noop, bufferSize: sz }; }
}
global.window = { addEventListener: noop, removeEventListener: noop, AudioContext: MockAudioContext };
global.AudioContext = MockAudioContext;
global.requestAnimationFrame = noop;
global.performance = { now: () => 0 };
""" + js + """
if (ONSET_NAMES.length !== 45) throw new Error("ONSET_NAMES length=" + ONSET_NAMES.length);
if (CODA_NAMES.length !== 30) throw new Error("CODA_NAMES length=" + CODA_NAMES.length);
if (ONSET_META.length !== 45) throw new Error("ONSET_META length=" + ONSET_META.length);
for (const [pname, ph] of Object.entries(PHRASES)) {
  if (ph.steps.length !== 16) throw new Error(pname + " steps=" + ph.steps.length);
  ph.steps.forEach((s, idx) => {
    if (!s.tie && !s.rest) {
      if (s.onset !== undefined && (s.onset < 0 || s.onset >= ONSET_NAMES.length))
        throw new Error(pname + "[" + idx + "] bad onset " + s.onset);
      if (s.coda !== undefined && (s.coda < 0 || s.coda >= CODA_NAMES.length))
        throw new Error(pname + "[" + idx + "] bad coda " + s.coda);
    }
  });
}
if (typeof selectPhrase !== 'function' || typeof applyCreature !== 'function' || typeof drawLCD !== 'function')
  throw new Error("Missing key simulator functions");
for (const pname of Object.keys(PHRASES)) {
  selectPhrase(pname);
}
for (const [fam, pages] of Object.entries(PAGES)) {
  state.pageFam = fam;
  for (let pi = 0; pi < pages.length; pi++) {
    state.pageIndex = pi;
    updateKnobDisplays();
    drawLCD();
  }
}
triggerReSpeak();
noteOn(60, 105);
const buf = new Float32Array(512);
const audioEvt = { outputBuffer: { getChannelData: () => buf } };
for (let c = 0; c < 4; c++) {
  applyCreature(c);
  for (let onset = 0; onset < ONSET_NAMES.length; onset++) {
    state.p.onset = onset;
    state.morph.elapsedSamples = 0;
    processAudio(audioEvt);
    for (let i = 0; i < buf.length; i++) {
      if (!Number.isFinite(buf[i]) || Math.abs(buf[i]) > 1.0) {
        throw new Error("Audio unstable creature=" + c + " onset=" + onset + " sample=" + buf[i]);
      }
    }
  }
}
drawLCD();
noteOff(60);
togglePlay();
for (let step = 0; step < 16; step++) {
  stepSequencer();
  processAudio(audioEvt);
  drawLCD();
}
togglePlay();
console.log("SIM_OK phrases=" + Object.keys(PHRASES).join(",") + "|onsets=" + ONSET_NAMES.length + " codas=" + CODA_NAMES.length);
""")
        js_path = Path(tf.name)

    try:
        subprocess.run(["node", "--check", str(js_path)], check=True)
        res = subprocess.run(["node", str(js_path)], capture_output=True, text=True)
        if res.returncode != 0:
            print("NODE STDERR:", res.stderr)
            print("NODE STDOUT:", res.stdout)
            raise RuntimeError(f"node failed with {res.returncode}")
        print(res.stdout.strip())
        m = re.search(r"phrases=(.*?)\|onsets=", res.stdout)
        assert m, f"Unexpected simulator test output: {res.stdout}"
        js_phrases = set(m.group(1).split(","))
        assert js_phrases == data_phrases_html, f"Phrase mismatch HTML={data_phrases_html} vs JS={js_phrases}"
        print(f"DOM IDs matched: {len(js_ids)}, HTML phrases matched: {len(data_phrases_html)}")
    finally:
        js_path.unlink(missing_ok=True)


if __name__ == "__main__":
    main()
