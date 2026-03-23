# Security Policy

## Supported Versions

CLIX is still evolving quickly.

At the moment, security fixes are only guaranteed for:

- the current `main` branch
- the latest unreleased state of the project

Historical commits and older snapshots should be considered unsupported unless stated otherwise.

## What to Report

Please report vulnerabilities involving:

- unsafe parsing behavior
- config file handling issues
- environment-variable resolution issues
- command or completion injection risks
- path traversal or filesystem access issues
- denial-of-service style parser bugs
- memory safety concerns

If you are unsure whether something is security-relevant, report it anyway.

## How to Report

Please do not open public issues for suspected vulnerabilities.

Instead, report them privately to:

- `is.kkokotero@gmail.com`

When possible, include:

- a clear description of the issue
- affected commit, branch, or version
- reproduction steps
- proof of concept or sample input
- expected impact
- any suggested remediation

## Response Expectations

The project will try to:

- acknowledge reports within 72 hours
- provide an initial assessment within 7 days when practical
- coordinate a fix before public disclosure

These are goals, not guarantees, especially while the project is still small.

## Disclosure

Please allow time for coordinated remediation before disclosing a vulnerability publicly.

Once a fix is available, the project may publish:

- a summary of the issue
- affected scope
- remediation guidance
- any compatibility notes

## Security Design Notes

CLIX tries to reduce risk by:

- staying header-only and dependency-light
- preferring explicit parsing rules
- failing fast on malformed input
- validating config file formats and extensions
- keeping completion and config behavior schema-driven

That said, no parser or runtime should be assumed to be risk-free. Responsible reports are appreciated.
