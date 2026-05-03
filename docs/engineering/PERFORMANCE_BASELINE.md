# Performance Baseline

Last updated: 2026-05-03

## Current status

No complete reproducible benchmark baseline exists in this repo document yet.

## Required benchmark dimensions

- Hardware and OS.
- Model name and model hash.
- Audio fixture name, duration, sample rate, and language.
- Time to first token if streaming exists.
- Time to final transcript.
- End-to-end insertion latency.
- Word error rate or expected transcript comparison where possible.
- CPU/RAM/battery impact where relevant.

## First proposed benchmark slice

Add a small local command that runs one or more known audio fixtures through the desktop inference pipeline and writes JSON with environment, model, timings, and transcript output. Do not publish speed claims until this exists.
