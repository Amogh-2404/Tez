#!/usr/bin/env python3
"""Publish the versioned Docker Hub overview using repository credentials."""
import json
import os
from pathlib import Path
import sys
import urllib.error
import urllib.request

API = 'https://hub.docker.com/v2'
REPOSITORY = 'ramogh2404/tez'
DESCRIPTION = 'A compact C++17 HTTP server built on Boost.Beast and Boost.Asio.'


def request(path, method='GET', payload=None, token=None):
    headers = {'Content-Type': 'application/json'}
    if token:
        headers['Authorization'] = f'Bearer {token}'
    data = None if payload is None else json.dumps(payload).encode()
    req = urllib.request.Request(API + path, data=data, headers=headers, method=method)
    with urllib.request.urlopen(req, timeout=30) as response:
        content = response.read()
        return json.loads(content) if content else {}


def main():
    username = os.environ.get('DOCKER_USERNAME')
    password = os.environ.get('DOCKER_PASSWORD')
    if not username or not password:
        raise RuntimeError('DOCKER_USERNAME and DOCKER_PASSWORD must be configured.')
    overview = (Path(__file__).resolve().parents[1] / 'docs/dockerhub.md').read_text()
    if not overview.strip() or len(overview.encode()) > 25000:
        raise RuntimeError('Docker Hub overview must contain 1..25000 bytes.')
    token = request('/auth/token', 'POST', {'identifier': username, 'secret': password})['access_token']
    if os.environ.get('GITHUB_ACTIONS') == 'true':
        print(f'::add-mask::{token}', flush=True)
    route = f'/repositories/{REPOSITORY}/'
    request(route, 'PATCH', {'description': DESCRIPTION, 'full_description': overview}, token)
    published = request(route)
    if published.get('description') != DESCRIPTION or published.get('full_description') != overview:
        raise RuntimeError('Docker Hub did not return the expected overview after publication.')
    print(f'Updated and verified the {REPOSITORY} overview.')


if __name__ == '__main__':
    try:
        main()
    except urllib.error.HTTPError as error:
        # Authentication responses can contain credentials; never dump their body.
        print(f'Docker Hub request failed with HTTP {error.code}.', file=sys.stderr)
        sys.exit(1)
    except (RuntimeError, KeyError, OSError, ValueError) as error:
        print(f'Docker Hub sync failed: {error}', file=sys.stderr)
        sys.exit(1)
