# `website/tools/` — offline content-generation helpers

This directory holds small scripts that produce content for the static
website, but which are **not** part of the website's build or runtime
pipeline. Files here are invoked by hand when an asset needs (re)generating.

## `render-ember-hero.py`

Renders the *ember* product hero image used on the website's home page and
in the README. It calls the MiniMax `image-01` image-generation API with
a fixed prompt that emphasizes the 4×4 step-button matrix as the device's
hero feature, and uses the public PO-33 product photo on
`muzikercdn.com` as a visual reference.

### When to use it

- The *ember* hardware description changes in `docs/DESIGN.md` and the
  current hero image no longer matches.
- The brand palette / finish / "ember" accent strip changes and the
  existing render needs a refresh.
- A website-team reviewer asks for a higher-resolution or alternative-angle
  version of the same product.

The script is **not** run as part of `npm run build`. It is invoked by a
human, the resulting image is reviewed, and only then is it copied into
`website/src/assets/img/` (or wherever the consumer expects it) and
committed to the repo as a regular asset.

### Prerequisites

- Python 3.8 or newer
- The `requests` package (`pip install requests`)
- A valid MiniMax API key with image-generation quota. The key is **not**
  stored in this repo; it is read from the `MINIMAX_API_KEY` env var at
  runtime.

### Environment variables

| Variable | Required | Default | Purpose |
|---|---|---|---|
| `MINIMAX_API_KEY` | **yes** | — | MiniMax Coding Plan key. The script refuses to start without it. |
| `IMAGE_REFERENCE_URL` | no | public PO-33 photo on `muzikercdn.com` | Public HTTPS URL used as the visual reference for the render. Override to point at a different reference image. |
| `OUTPUT_PATH` | no | `./ember-hero.png` (cwd) | Where to write the rendered PNG. The first image in a multi-image response is written here; any additional images use `output_N.jpeg`. |
| `LOCAL_WEBP_PATH` | no | `reference/po33.webp` | Informational only. The script prints whether this local file exists. The API does **not** read it. |

### Usage

```bash
# from the website/ directory
export MINIMAX_API_KEY='sk-cp-...'

# default: writes ./ember-hero.png in the current directory
python3 tools/render-ember-hero.py

# write to a specific path
OUTPUT_PATH=src/assets/img/ember-hero.png python3 tools/render-ember-hero.py

# use a different reference image
IMAGE_REFERENCE_URL='https://example.com/your-reference.jpg' \
    python3 tools/render-ember-hero.py
```

### What gets sent to the API

The script sends a single JSON POST to the API endpoint in the file's
`API_URL` constant (currently `https://api.minimaxi.com/v1/image_generation`
— **see the inline `NOTE: verify host; expected global endpoint is
api.minimax.io` comment in the script**; the `.com` host should be
confirmed before the first real run). The request body contains:

- `model`: `image-01`
- `prompt`: the in-script `PROMPT` constant (currently 1182 chars; under
  the documented 1500-char limit)
- `aspect_ratio`: `16:9`
- `response_format`: `base64`
- `subject_reference`: a single entry of type `character` pointing at the
  reference URL

The local WebP file at `LOCAL_WEBP_PATH` is **not** uploaded. The script
uses the public URL above because most MiniMax plans reject `data:` URLs
in `subject_reference.image_file`.

### Outputs

- A single PNG (or the first of N if the API returns more than one) at
  `OUTPUT_PATH`.
- A short transcript on stdout showing the reference URL that was used
  and the path each image was written to.
- On API error, an error message and the offending response body.

### Security

- The script never writes the API key to disk, never logs it, and never
  embeds it in any HTTP body. The key is read once from the env var and
  used in a single `Authorization: Bearer …` header.
- `.env` and rendered images in this directory are git-ignored so an
  accidental `git add -A` cannot commit them.
- The script does **not** call out to anywhere except the `API_URL`
  constant. Network egress is to a single host.

### What is **not** in this script

- No retry logic. A transient API failure will surface as an error; the
  caller decides whether to re-run.
- No image post-processing. The render is saved as the API returned it.
- No rate limiting. The MiniMax Coding Plan has its own quota; exceeding
  it returns `2049` or similar and the script reports the error.
- No CLI argument parsing. All inputs are environment variables. If a
  more complex interface is needed, switch to `click` or `argparse` and
  move the `os.environ` reads into a `main()`-level block.

### Known issues / follow-ups

- The `API_URL` constant currently points at `api.minimaxi.com`. The
  documented global endpoint is `api.minimax.io`. The first maintainer
  to use this script should verify the host before running; the inline
  `NOTE` in the script flags this.
- The `encode_local_image_to_data_url` helper function is dead code
  left over from an earlier version. It is not called anywhere. Safe
  to delete in a follow-up commit.
- The `sk-cp-...` placeholder strings in the `KeyError` exit message
  leak the key's prefix family. Acceptable because the prefix alone
  is not a secret, and the message is only shown when the key is
  missing entirely.
