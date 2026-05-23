# Security Policy

## Supported versions

Security fixes are applied to the active development branch and included in the next release.

## Reporting a vulnerability

Please do not open public GitHub issues for vulnerabilities.

Report privately to the maintainers with:

- affected component and target (core_s3 or timer_camera_f)
- reproduction steps
- impact summary
- proof-of-concept if available

Acknowledgement target: within 5 business days.

## Secret handling requirements

- No plaintext secrets in tracked files.
- Use include/secrets.local.h for local credentials.
- Never commit keys, passwords, or tokens.
- Rotate credentials immediately if exposure is suspected.

## Security release process

1. Confirm issue and scope.
2. Prepare fix in private branch if needed.
3. Rotate exposed credentials where applicable.
4. Validate with build and secret scan.
5. Publish fix with advisory summary.
