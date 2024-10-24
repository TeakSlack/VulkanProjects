import tarfile
import zipfile
import os
import requests

import urllib.error
import urllib.request

try:
    from tqdm import tqdm
except ImportError:
    print(f"Package tqdm is not installed.")
    tqdm = None

def DownloadFile(url, filePath):
    path = filePath
    filePath = os.path.abspath(filePath)
    fileName = os.path.basename(path)
    os.makedirs(os.path.dirname(filePath), exist_ok=True)

    headers = {'User-Agent': "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_4) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/83.0.4103.97 Safari/537.36"}
    try:
        with open(filePath, 'wb') as f:
            response = requests.get(url, headers=headers, stream=True)
            total = response.headers.get('content-length')
            response.raise_for_status()

            if tqdm is not None:
                progressBar = tqdm(total=int(total) - 1, unit='B', unit_scale=True, desc=fileName)

            if total is None:
                f.write(response.content)
            else:
                for chunk in response.iter_content(chunk_size=8192):
                    f.write(chunk)
                    progressBar.update(len(chunk))

        if tqdm is not None:
            progressBar.close()

    except urllib.error.URLError as e:
        print(f"URL Error encountered: {e.reason}")
        os.remove(filePath)
    except urllib.error.HTTPError as e:
        print(f"HTTP Error  encountered: {e.code} - {e.reason}")
        os.remove(filePath)
    except Exception as e:
        print(f"An error occurred: {str(e)}")
        os.remove(filePath)

def TarFile(filePath, deleteTar=True, type='gz'):
    path = filePath
    fileName = os.path.basename(path)
    filePath = os.path.abspath(filePath)
    with tarfile.open(filePath, 'r:' + type) as tar_ref:
        members = tar_ref.getmembers()
        totalSize = sum(member.size for member in members)

        if tqdm is not None:
            progressBar = tqdm(total=totalSize - 1, desc='Extracting', unit='B', unit_scale=True)
        for member in members:
            untarredPath = os.path.abspath(f"{os.path.dirname(filePath)}")
            tar_ref.extract(member, path=untarredPath)
            if tqdm is not None:
                progressBar.update(member.size)
        if tqdm is not None:
            progressBar.close()

    if deleteTar:
        os.remove(filePath)

    print(f"Extracted {fileName}")
    
def ZipFile(filePath, deleteZip=True):
    path = filePath
    fileName = os.path.basename(path)
    filePath = os.path.abspath(filePath)
    with zipfile.ZipFile(filePath, 'r') as zip_ref:
        zipFiles = zip_ref.namelist()
        totalSize = sum(zip_ref.getinfo(file).file_size for file in zipFiles)
        if tqdm is not None:
            progressBar = tqdm(total=totalSize - 1, desc='Extracting', unit='B', unit_scale=True)

        unzippedPath = os.path.abspath(f"{os.path.dirname(filePath)}")
        for file in zipFiles:
            zip_ref.extract(file, path=unzippedPath)
            if tqdm is not None:
                progressBar.update(zip_ref.getinfo(file).file_size)
        if tqdm is not None:
            progressBar.close()
    
    if deleteZip:
        os.remove(filePath)

    print(f"Extracted {fileName}")