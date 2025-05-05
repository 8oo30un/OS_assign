# `wspipe` - Pipe-Based Word Search Program

## Overview

`wspipe`는 사용자가 입력한 명령을 실행하고, 그 결과에서 특정 단어를 찾아서 해당 단어를 강조 표시하는 프로그램입니다. 이 프로그램은 Linux의 `cat`과 `grep` 명령을 사용하는 것처럼 동작하며, 파이프를 사용하여 프로세스 간 통신을 합니다. 또한, 특정 범위의 줄에 대해서만 검색을 수행할 수 있는 옵션도 지원합니다.

## Features

1. **명령 실행**: 사용자 입력에 따라 자식 프로세스가 명령을 실행합니다.
2. **파이프 사용**: 부모 프로세스는 자식 프로세스의 출력을 파이프를 통해 받아옵니다.
3. **단어 강조**: 검색된 단어는 빨간색으로 강조 표시됩니다.
4. **라인 범위 지정**: `--line-range` 옵션을 사용하여 검색할 줄 범위를 지정할 수 있습니다. - _creative service_
5. **사용자 정의 문자열 함수**: `strtok`, `strcmp`, `strstr` 등의 표준 라이브러리 함수를 직접 구현하여 사용합니다.

## Usage

### 기본 사용법

```bash
./wspipe "<command>" <search_word>

./wspipe --line-range <start> <end> "<command>" <search_word>

• <start>: 검색을 시작할 줄 번호
• <end>: 검색을 종료할 줄 번호
```
