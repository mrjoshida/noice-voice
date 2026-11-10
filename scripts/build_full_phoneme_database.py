#!/usr/bin/env python3
"""Build complete phoneme database for NOICE VOICE in assets/noice/chimera_geometry.json."""
import json
from pathlib import Path

path = Path(__file__).resolve().parents[1] / "assets" / "noice" / "chimera_geometry.json"
with open(path) as f:
    d = json.load(f)

male_cons = d['bodies']['male']['consonants']

# Helper to create smooth 22-section area profiles with a constriction
def make_areas(constr_sec, min_a, pharynx_a=2.8, oral_a=3.5, lip_a=2.5):
    areas = []
    for i in range(22):
        if i < 8:
            base = pharynx_a + (i / 8.0) * 0.8
        elif i < 18:
            base = oral_a - ((i - 8) / 10.0) * 0.5
        else:
            base = lip_a - ((i - 18) / 3.0) * (lip_a - 1.2)
        dist = abs(i - constr_sec)
        if dist == 0:
            a = min_a
        elif dist == 1:
            a = min_a + (base - min_a) * 0.35
        elif dist == 2:
            a = min_a + (base - min_a) * 0.70
        else:
            a = base
        areas.append(round(max(0.01, min(6.5, a)), 3))
    return areas

# 1. New Core Stops
# /b/
male_cons["b"] = {
    "ipa": "b", "name": "voiced bilabial stop", "manner": "stop", "voicing": "voiced",
    "dur": 0.07, "nasal": False, "constriction_idx": 21, "min_area": 0.01,
    "target_areas_22": make_areas(21, 0.01, pharynx_a=3.2, oral_a=4.0, lip_a=0.01)
}
# /d/
male_cons["d"] = {
    "ipa": "d", "name": "voiced alveolar stop", "manner": "stop", "voicing": "voiced",
    "dur": 0.06, "nasal": False, "constriction_idx": 16, "min_area": 0.01,
    "target_areas_22": make_areas(16, 0.01, pharynx_a=3.2, oral_a=3.5, lip_a=2.0)
}
# /g/
male_cons["g"] = {
    "ipa": "g", "name": "voiced velar stop", "manner": "stop", "voicing": "voiced",
    "dur": 0.07, "nasal": False, "constriction_idx": 8, "min_area": 0.01,
    "target_areas_22": make_areas(8, 0.01, pharynx_a=2.2, oral_a=3.8, lip_a=2.2)
}
# /k/
male_cons["k"] = {
    "ipa": "k", "name": "voiceless velar stop", "manner": "stop", "voicing": "voiceless",
    "dur": 0.08, "nasal": False, "constriction_idx": 8, "min_area": 0.01,
    "target_areas_22": make_areas(8, 0.01, pharynx_a=2.0, oral_a=3.8, lip_a=2.2)
}
# Glottal Stop [ʔ]
male_cons["ʔ"] = {
    "ipa": "ʔ", "name": "glottal stop", "manner": "glottal", "voicing": "voiceless",
    "dur": 0.06, "nasal": False, "constriction_idx": 0, "min_area": 0.01,
    "target_areas_22": make_areas(0, 0.01, pharynx_a=1.5, oral_a=3.5, lip_a=2.5)
}
# Uvular Stop [q]
male_cons["q"] = {
    "ipa": "q", "name": "voiceless uvular stop", "manner": "stop", "voicing": "voiceless",
    "dur": 0.08, "nasal": False, "constriction_idx": 5, "min_area": 0.01,
    "target_areas_22": make_areas(5, 0.01, pharynx_a=1.8, oral_a=3.6, lip_a=2.4)
}

# 2. Dental & Postalveolar Fricatives
# Voiceless Dental Fricative [θ] ("th" in thin)
male_cons["θ"] = {
    "ipa": "θ", "name": "voiceless dental fricative", "manner": "fricative", "voicing": "voiceless",
    "dur": 0.11, "nasal": False, "constriction_idx": 18, "min_area": 0.15,
    "target_areas_22": make_areas(18, 0.15, pharynx_a=2.6, oral_a=3.4, lip_a=1.8)
}
# Voiced Dental Fricative [ð] ("th" in this)
male_cons["ð"] = {
    "ipa": "ð", "name": "voiced dental fricative", "manner": "fricative", "voicing": "voiced",
    "dur": 0.07, "nasal": False, "constriction_idx": 18, "min_area": 0.17,
    "target_areas_22": make_areas(18, 0.17, pharynx_a=2.8, oral_a=3.4, lip_a=1.8)
}
# Voiced Postalveolar Fricative [ʒ] ("measure")
male_cons["ʒ"] = {
    "ipa": "ʒ", "name": "voiced postalveolar fricative", "manner": "fricative", "voicing": "voiced",
    "dur": 0.09, "nasal": False, "constriction_idx": 16, "min_area": 0.22,
    "target_areas_22": make_areas(16, 0.22, pharynx_a=2.8, oral_a=3.2, lip_a=1.5)
}
# Voiceless Velar Fricative [x] ("loch")
male_cons["x"] = {
    "ipa": "x", "name": "voiceless velar fricative", "manner": "fricative", "voicing": "voiceless",
    "dur": 0.12, "nasal": False, "constriction_idx": 8, "min_area": 0.18,
    "target_areas_22": make_areas(8, 0.18, pharynx_a=2.0, oral_a=3.6, lip_a=2.2)
}
# Voiced Uvular Fricative [ʁ] (French r)
male_cons["ʁ"] = {
    "ipa": "ʁ", "name": "voiced uvular fricative", "manner": "fricative", "voicing": "voiced",
    "dur": 0.09, "nasal": False, "constriction_idx": 5, "min_area": 0.25,
    "target_areas_22": make_areas(5, 0.25, pharynx_a=1.6, oral_a=3.5, lip_a=2.2)
}
# Voiceless Pharyngeal Fricative [ħ] (Arabic haa)
male_cons["ħ"] = {
    "ipa": "ħ", "name": "voiceless pharyngeal fricative", "manner": "fricative", "voicing": "voiceless",
    "dur": 0.11, "nasal": False, "constriction_idx": 2, "min_area": 0.30,
    "target_areas_22": make_areas(2, 0.30, pharynx_a=1.2, oral_a=3.8, lip_a=2.5)
}
# Voiced Pharyngeal Fricative [ʕ] (Arabic ayn)
male_cons["ʕ"] = {
    "ipa": "ʕ", "name": "voiced pharyngeal fricative", "manner": "fricative", "voicing": "voiced",
    "dur": 0.09, "nasal": False, "constriction_idx": 2, "min_area": 0.35,
    "target_areas_22": make_areas(2, 0.35, pharynx_a=1.4, oral_a=3.8, lip_a=2.5)
}

# 3. Affricates
# Voiceless Postalveolar Affricate [tʃ] ("church")
male_cons["tʃ"] = {
    "ipa": "tʃ", "name": "voiceless postalveolar affricate", "manner": "affricate", "voicing": "voiceless",
    "dur": 0.13, "nasal": False, "constriction_idx": 16, "min_area": 0.08,
    "target_areas_22": make_areas(16, 0.08, pharynx_a=2.5, oral_a=3.2, lip_a=1.6)
}
# Voiced Postalveolar Affricate [dʒ] ("judge")
male_cons["dʒ"] = {
    "ipa": "dʒ", "name": "voiced postalveolar affricate", "manner": "affricate", "voicing": "voiced",
    "dur": 0.12, "nasal": False, "constriction_idx": 16, "min_area": 0.10,
    "target_areas_22": make_areas(16, 0.10, pharynx_a=2.7, oral_a=3.2, lip_a=1.6)
}
# Voiceless Alveolar Affricate [ts]
male_cons["ts"] = {
    "ipa": "ts", "name": "voiceless alveolar affricate", "manner": "affricate", "voicing": "voiceless",
    "dur": 0.12, "nasal": False, "constriction_idx": 17, "min_area": 0.08,
    "target_areas_22": make_areas(17, 0.08, pharynx_a=2.6, oral_a=3.4, lip_a=2.0)
}

# 4. Extended Retroflex Family
# Retroflex Stop [ʈ]
male_cons["ʈ"] = {
    "ipa": "ʈ", "name": "voiceless retroflex stop", "manner": "stop", "voicing": "voiceless",
    "dur": 0.075, "nasal": False, "constriction_idx": 15, "min_area": 0.01,
    "target_areas_22": make_areas(15, 0.01, pharynx_a=2.0, oral_a=2.8, lip_a=1.8)
}
# Retroflex Voiced Stop [ɖ]
male_cons["ɖ"] = {
    "ipa": "ɖ", "name": "voiced retroflex stop", "manner": "stop", "voicing": "voiced",
    "dur": 0.065, "nasal": False, "constriction_idx": 15, "min_area": 0.01,
    "target_areas_22": make_areas(15, 0.01, pharynx_a=2.2, oral_a=2.8, lip_a=1.8)
}
# Retroflex Nasal [ɳ]
male_cons["ɳ"] = {
    "ipa": "ɳ", "name": "retroflex nasal", "manner": "nasal", "voicing": "voiced",
    "dur": 0.07, "nasal": True, "constriction_idx": 15, "min_area": 0.01,
    "target_areas_22": make_areas(15, 0.01, pharynx_a=2.4, oral_a=2.8, lip_a=1.8)
}
# Retroflex Fricative [ʂ]
male_cons["ʂ"] = {
    "ipa": "ʂ", "name": "voiceless retroflex fricative", "manner": "fricative", "voicing": "voiceless",
    "dur": 0.12, "nasal": False, "constriction_idx": 15, "min_area": 0.15,
    "target_areas_22": make_areas(15, 0.15, pharynx_a=2.0, oral_a=2.8, lip_a=1.8)
}
# Retroflex Lateral [ɭ]
male_cons["ɭ"] = {
    "ipa": "ɭ", "name": "retroflex lateral", "manner": "lateral", "voicing": "voiced",
    "dur": 0.065, "nasal": False, "constriction_idx": 15, "min_area": 0.35,
    "target_areas_22": make_areas(15, 0.35, pharynx_a=2.2, oral_a=3.0, lip_a=2.0)
}
# Alveolar Tap / Flap [ɾ] ("water")
male_cons["ɾ"] = {
    "ipa": "ɾ", "name": "alveolar tap", "manner": "tap", "voicing": "voiced",
    "dur": 0.025, "nasal": False, "constriction_idx": 16, "min_area": 0.05,
    "target_areas_22": make_areas(16, 0.05, pharynx_a=2.8, oral_a=3.4, lip_a=2.2)
}
# Palatal Nasal [ɲ] ("canyon")
male_cons["ɲ"] = {
    "ipa": "ɲ", "name": "palatal nasal", "manner": "nasal", "voicing": "voiced",
    "dur": 0.075, "nasal": True, "constriction_idx": 14, "min_area": 0.01,
    "target_areas_22": make_areas(14, 0.01, pharynx_a=2.5, oral_a=2.2, lip_a=2.2)
}

# 5. Beatbox Non-Pulmonic & Percussive Airstreams
# Ejective Kick Drum [B] (p')
male_cons["B"] = {
    "ipa": "B", "name": "beatbox kick", "manner": "stop", "voicing": "voiceless",
    "dur": 0.04, "nasal": False, "constriction_idx": 21, "min_area": 0.01,
    "target_areas_22": make_areas(21, 0.01, pharynx_a=4.5, oral_a=4.5, lip_a=0.01)
}
# Beatbox Snare [Pf]
male_cons["Pf"] = {
    "ipa": "Pf", "name": "beatbox snare", "manner": "affricate", "voicing": "voiceless",
    "dur": 0.09, "nasal": False, "constriction_idx": 19, "min_area": 0.08,
    "target_areas_22": make_areas(19, 0.08, pharynx_a=3.0, oral_a=3.2, lip_a=0.1)
}
# Closed Hi-Hat [t]
male_cons["HIHAT"] = {
    "ipa": "HIHAT", "name": "closed hi-hat", "manner": "fricative", "voicing": "voiceless",
    "dur": 0.035, "nasal": False, "constriction_idx": 17, "min_area": 0.06,
    "target_areas_22": make_areas(17, 0.06, pharynx_a=2.2, oral_a=3.0, lip_a=2.0)
}
# Ejective Rimshot [K]
male_cons["K"] = {
    "ipa": "K", "name": "beatbox rimshot", "manner": "stop", "voicing": "voiceless",
    "dur": 0.045, "nasal": False, "constriction_idx": 8, "min_area": 0.01,
    "target_areas_22": make_areas(8, 0.01, pharynx_a=3.5, oral_a=3.0, lip_a=2.5)
}
# Tongue Click [!] (Velaric pop)
male_cons["!"] = {
    "ipa": "!", "name": "tongue click", "manner": "stop", "voicing": "voiceless",
    "dur": 0.03, "nasal": False, "constriction_idx": 16, "min_area": 0.08,
    "target_areas_22": make_areas(16, 0.08, pharynx_a=2.5, oral_a=1.5, lip_a=2.5)
}
# Lip Pop [P!] (Cavitation pop)
male_cons["P!"] = {
    "ipa": "P!", "name": "lip pop", "manner": "stop", "voicing": "voiceless",
    "dur": 0.03, "nasal": False, "constriction_idx": 21, "min_area": 0.05,
    "target_areas_22": make_areas(21, 0.05, pharynx_a=3.5, oral_a=4.0, lip_a=0.05)
}

# 6. Complete Curated Ordered Lists of Onsets and Codas
ordered_onsets = [
    # Core Stops (1..6)
    "p", "b", "t", "d", "k", "g",
    # Nasals (7..10)
    "m", "n", "ŋ", "ɲ",
    # Fricatives (11..19)
    "f", "v", "θ", "ð", "s", "z", "ʃ", "ʒ", "h",
    # Affricates (20..22)
    "tʃ", "dʒ", "ts",
    # Liquids & Trills (23..29)
    "w", "j", "l", "r", "ɻ", "ɹ", "ɾ",
    # Retroflex & Guttural (30..36)
    "ʈ", "ɖ", "ʂ", "x", "q", "ʁ", "ħ", "ʕ", "ʔ",
    # Beatbox Percussion (37..42)
    "B", "Pf", "HIHAT", "K", "!", "P!"
]

ordered_codas = [
    # Nasals
    "m", "n", "ŋ", "ɲ",
    # Liquids
    "l", "r", "ɻ", "ɹ", "ɾ",
    # Fricatives
    "s", "z", "ʃ", "ʒ", "θ", "ð", "f", "v",
    # Stops
    "p", "t", "k", "b", "d", "g",
    # Affricates & Glottals
    "tʃ", "dʒ", "h", "ʔ",
    # Clicks
    "!", "P!"
]

d['onset_list'] = ordered_onsets
d['coda_list'] = ordered_codas

with open(path, "w") as f:
    json.dump(d, f, indent=2)

print(f"Successfully generated database! Onsets: {len(ordered_onsets)}, Codas: {len(ordered_codas)}")
