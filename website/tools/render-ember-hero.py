import base64
import sys
import requests
import os
import mimetypes

# ================= Configuration =================
# Authentication: this script reads the MiniMax API key from the
# MINIMAX_API_KEY environment variable. Set it in your shell before
# running (do NOT hard-code it in this file):
#     export MINIMAX_API_KEY='sk-cp-...'
#
# Optional overrides:
#   IMAGE_REFERENCE_URL  public HTTPS URL of a reference image used
#                        as the visual subject for the render. Defaults
#                        to the public PO-33 product photo on the
#                        muzikercdn.com CDN (the same asset that lives
#                        locally at LOCAL_WEBP_PATH).
#   OUTPUT_PATH          where to write the rendered PNG. Default is
#                        ./ember-hero.png in the current directory.
#
# Required Python packages: requests (install with: pip install requests)
try:
    API_KEY = os.environ["MINIMAX_API_KEY"]
except KeyError:
    sys.exit("ERROR: MINIMAX_API_KEY env var is not set. "
             "Export it before running, e.g.: export MINIMAX_API_KEY='sk-cp-...'")

PROMPT = """\
Product photograph of a pocket-sized groovebox sampler prototype, three-\
quarter hero angle, in-hand on a soft dark gradient. Hero feature: a square \
4x4 matrix of exactly sixteen rubber step buttons in four rows and four \
columns. Every row has four buttons and every column has four buttons. The \
matrix is recessed into the front face, each button flush-lit by a soft \
amber underglow, and is the dominant element of the composition in the \
lower-right two-thirds of the body. Render the grid as a square, render all \
sixteen buttons, and do not collapse any row or column. Thin warm copper-\
orange "ember" accent strip traces the grid perimeter. Modernized pocket \
operator descendant: flat slim credit-card-shaped matte-black aluminum body, \
chamfered edge, no visible screws, USB-C on the side. Two round knobs upper-\
right with metal shafts and colored rings (one amber, one white). Six to \
seven round function buttons along the right and bottom-right edge. A 1.8-\
inch color TFT upper-left (replacing the segment LCD) showing a soft amber \
waveform. Flip-out aluminum stand/clip on the back. Studio lighting, sharp \
product-render finish, photoreal, cinematic, no text or logos on device.\
"""
LOCAL_WEBP_PATH = os.environ.get("LOCAL_WEBP_PATH", "reference/po33.webp")  # Informational only; the API uses the URL below.

# Global endpoint: https://api.minimax.io/v1/image_generation
# China endpoint: https://api.minimax.cn/v1/image_generation
API_URL = "https://api.minimaxi.com/v1/image_generation"  # NOTE: verify host; expected global endpoint is api.minimax.io
OUTPUT_PATH = os.environ.get("OUTPUT_PATH", "ember-hero.png")

# Public HTTPS URL of the PO-33 reference image. Used as the
# subject_reference.image_file value (preferred over a data: URL,
# which most MiniMax plans reject). Override with the
# IMAGE_REFERENCE_URL env var to point at a different reference.
DEFAULT_IMAGE_REFERENCE_URL = "https://muzikercdn.com/uploads/product_gallery/5119/511922/thumb_d_gallery_base_9cfda642.jpg"
# ===========================================

def encode_local_image_to_data_url(image_path):
    """Encode local image as Base64 Data URL"""
    if not os.path.exists(image_path):
        raise FileNotFoundError(f"File not found: {image_path}")
    
    # Infer MIME type (webp should be recognized as image/webp)
    mime_type, _ = mimetypes.guess_type(image_path)
    if mime_type is None:
        # If inference fails, based on file extension, force specification (for WebP)
        if image_path.lower().endswith(".webp"):
            mime_type = "image/webp"
        else:
            mime_type = "application/octet-stream"
    
    with open(image_path, "rb") as f:
        encoded = base64.b64encode(f.read()).decode("utf-8")
    
    return f"data:{mime_type};base64,{encoded}"

def main():
    headers = {
        "Authorization": f"Bearer {API_KEY}",
        "Content-Type": "application/json"
    }
    
    # 1. Sanity-check that the local reference file exists, if set.
    #    The script no longer sends the local file to the API; it uses
    #    DEFAULT_IMAGE_REFERENCE_URL (or the IMAGE_REFERENCE_URL override)
    #    as a public HTTPS URL. This block is just a developer-facing
    #    diagnostic and a no-op failure.
    if os.path.isfile(LOCAL_WEBP_PATH):
        try:
            mime_type, _ = mimetypes.guess_type(LOCAL_WEBP_PATH)
            print(f"Local reference found at {LOCAL_WEBP_PATH} (MIME: {mime_type or 'unknown'})")
        except Exception as e:
            print(f"Warning: could not inspect local reference: {e}")
    else:
        print(f"Note: local reference not found at {LOCAL_WEBP_PATH}; this is fine, the API uses the public URL below.")
    
    # 2. Construct request body.
    # The MiniMax image-01 subject_reference.image_file field expects a
    # public HTTPS URL, not a data: URL. Use IMAGE_REFERENCE_URL if set,
    # otherwise fall back to DEFAULT_IMAGE_REFERENCE_URL from the config
    # block above. We no longer send a data: URL.
    reference_url = os.environ.get("IMAGE_REFERENCE_URL", "").strip() or DEFAULT_IMAGE_REFERENCE_URL
    if not reference_url.lower().startswith(("http://", "https://")):
        print("ERROR: reference URL must be http(s); got:", reference_url)
        return
    image_file = reference_url
    print(f"Using subject_reference image URL: {image_file}")

    payload = {
        "model": "image-01",
        "prompt": PROMPT,
        "aspect_ratio": "16:9",
        "response_format": "base64",  # Using base64 to avoid URL expiration issues
        "subject_reference": [
            {
                "type": "character",
                "image_file": image_file
            }
        ]
    }
    
    # 3. Send request
    print("🚀 Sending request...")
    try:
        response = requests.post(API_URL, headers=headers, json=payload, timeout=60)
        response.raise_for_status()
        result = response.json()
        
        # 4. Check MiniMax application-layer error codes (HTTP 200 may still contain errors)
        base_resp = result.get("base_resp", {})
        status_code = base_resp.get("status_code", 0)
        if status_code != 0:
            print(f"❌ API returned error [{status_code}]: {base_resp.get('status_msg')}")
            print("💡 Tip: If the error indicates image format or URL issues, it means Data URL is not supported. Please upload the image to a public URL (such as an image hosting service) and replace image_file with that URL.")
            return
        
        # 5. Parse and save results
        images = result.get("data", {}).get("image_base64", [])
        if not images:
            print("⚠️ No image data in response.")
            print(result)
            return
        
        for i, b64 in enumerate(images):
            if i == 0:
                filename = OUTPUT_PATH
            else:
                filename = f"output_{i}.jpeg"
            with open(filename, "wb") as f:
                f.write(base64.b64decode(b64))
            print(f"Saved: {filename}")
            
    except requests.exceptions.RequestException as e:
        print(f"Network/HTTP error: {e}")
    except Exception as e:
        print(f"Unexpected error: {e}")

if __name__ == "__main__":
    main()
