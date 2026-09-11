# Release and repository maintenance

The development source, a registry tag, and a GitHub release are separate artifacts. Promote a version only when its code, documentation, tests, and images agree.

## Continuous integration

[ci.yml](../.github/workflows/ci.yml) checks Linux GCC with Boost 1.74, Linux Clang with AddressSanitizer/UndefinedBehaviorSanitizer, and macOS Apple Clang. It runs unit and wire-protocol tests, checks formatting, source-file whitespace and local documentation links, analyzes C++ with CodeQL, and smoke-tests native amd64/arm64 containers.

Test dependencies are required when `BUILD_TESTING=ON`. The CI jobs do not run performance benchmarks. Hosted-runner results are correctness evidence for those environments, not a performance baseline.

## Container publication

Publication runs on pushes to `main` or version tags after the build, quality, CodeQL, and initial container checks pass. It uses repository secrets `DOCKER_USERNAME` and `DOCKER_PASSWORD`. Pull-request builds do not receive registry credentials.

The workflow builds a multiarch image once and pushes it as `candidate-<full commit SHA>`. The binary version and OCI version label receive the same version string. Separate native amd64 and arm64 jobs then pull that exact candidate by digest and smoke-test its default content, mounted content, non-root/read-only operation, and signal-driven exit.

Only after both candidate checks pass does `docker buildx imagetools create` promote the tested manifest to the public aliases below. Promotion does not rebuild the image. Candidate tags are staging artifacts, not a deployment recommendation; a failed candidate check leaves the public aliases unchanged.

| Source | Intended container tags |
| --- | --- |
| Verified `main` push | `main`, `sha-<full commit SHA>` |
| Version tag such as `v1.1.0` | `1.1.0`, `1.1`, source SHA, and the metadata action's stable-release `latest` alias |
| Prerelease tag | Prerelease version and source SHA; check metadata before promotion |

Tag expansion follows the pinned [Docker metadata action](https://github.com/docker/metadata-action#typesemver); inspect its output when changing version policy.

Images target `linux/amd64` and `linux/arm64`, with source labels, provenance, and an SBOM requested from the candidate builder. Confirm the registry manifest and artifact metadata after publication; workflow configuration alone is not evidence that publication succeeded. Pin an image digest for an exact deployment.

A successful `main` image promotion runs [sync_dockerhub.py](../scripts/sync_dockerhub.py), which applies and verifies the short description and the maintained [Docker Hub overview](dockerhub.md). The overview must remain below the script's 25,000-byte limit. Update it with any changed flags, image paths, defaults, or feature boundaries.

## Versioned release

Before creating a version tag:

1. Review the diff and all required CI results for the intended source revision.
2. Move the relevant changelog entry out of `Unreleased`, set the release date, and align the version metadata and README status.
3. Check the image build and documented commands for that revision, including mounted config/static content.
4. Confirm license, architecture, runtime dependencies, security notes, and upgrade effects.
5. Create the version tag on that exact revision and let candidate build, digest verification, and promotion complete.
6. Inspect the promoted image manifest, source revision labels, digest, and generated release notes.

The workflow creates a **draft** GitHub release after tagged-image publication. Review the draft before publishing it. It does not attach native binary archives; do not advertise downloadable standalone Linux or macOS binaries that were not built and packaged. A dynamic binary copied out of a build job is not a portable distribution by itself.

If a check or publication fails, correct the problem and state which artifacts exist. Do not describe a failed image push or an unpublished draft as a release. Avoid moving an already published version tag to different source code.

## Repository presentation

The README uses [tez-banner.svg](assets/tez-banner.svg); Docker Hub uses its [PNG export](assets/tez-banner.png) through a public raw GitHub URL. Upload [tez-social.png](assets/tez-social.png) as the GitHub repository social preview; it is 1280×640. Its editable source is [tez-social.svg](assets/tez-social.svg). GitHub's social preview is a repository setting, so committing the PNG does not apply it automatically.

Suggested repository description:

> A compact C++17 HTTP server built on Boost.Beast and Boost.Asio.

Suggested topics: `cpp`, `cpp17`, `http-server`, `boost-asio`, `boost-beast`, `networking`, `systems-programming`.

Regenerate native SVG artwork after editing [generate_artwork.py](assets/generate_artwork.py):

```sh
python3 docs/assets/generate_artwork.py
rsvg-convert docs/assets/tez-social.svg -o docs/assets/tez-social.png
rsvg-convert docs/assets/tez-banner.svg -o docs/assets/tez-banner.png
```

`rsvg-convert` is provided by librsvg. Inspect every changed diagram and the banner at README display size. Keep alt text descriptive and update architecture text when components change. The sample homepage and About page are configured responses in [config.json](../config.json), styled by [static/style.css](../static/style.css).

Keep the current maintainer identity, standard Code of Conduct attribution, and project license intact. Use factual scope and evidence in descriptions; avoid unsupported performance, adoption, or security badges.
