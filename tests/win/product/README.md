# Product tests

Write each plugin's own checks here, in situ. The platform product runner discovers every `Test-*.ps1` file; there is no runner list to maintain. Register its Blocks test ID, script path, `scope: product`, CI eligibility and suite IDs in `tests/sequence.json`. Every script accepts the common options and writes one ID-named JSON result. An unregistered script is executed but fails the contract visibly, rather than disappearing from coverage.
