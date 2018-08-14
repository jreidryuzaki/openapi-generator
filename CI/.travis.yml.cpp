sudo: required
language: java
jdk:
  - openjdk8


cache:
  directories:
  - $HOME/.m2
  - $HOME/.ivy2
  - $HOME/.gradle/caches/
  - $HOME/.gradle/wrapper/
  - $HOME/samples/client/petstore/javascript/node_modules
  - $HOME/samples/client/petstore/php/OpenAPIToolsClient-php/vendor
  - $HOME/samples/client/petstore/ruby/vendor/bundle
  - $HOME/samples/client/petstore/python/.venv/
  - $HOME/samples/client/petstore/typescript-node/npm/node_modules
  - $HOME/samples/client/petstore/typescript-node/npm/typings/
  - $HOME/samples/client/petstore/typescript-fetch/tests/default/node_modules
  - $HOME/samples/client/petstore/typescript-fetch/tests/default/typings
  - $HOME/samples/client/petstore/typescript-fetch/builds/default/node_modules
  - $HOME/samples/client/petstore/typescript-fetch/builds/default/typings
  - $HOME/samples/client/petstore/typescript-fetch/builds/es6-target/node_modules
  - $HOME/samples/client/petstore/typescript-fetch/builds/es6-target/typings
  - $HOME/samples/client/petstore/typescript-fetch/builds/with-npm-version/node_modules
  - $HOME/samples/client/petstore/typescript-fetch/npm/with-npm-version/typings
  - $HOME/samples/client/petstore/typescript-angular/node_modules
  - $HOME/samples/client/petstore/typescript-angular/typings
  - $HOME/samples/server/petstore/rust-server/target
  - $HOME/perl5
  - $HOME/.cargo
  - $HOME/.stack
  - $HOME/samples/server/petstore/cpp-pistache/pistache

services:
  - docker

# comment out the host table change to use the public petstore server
addons:
  apt:
    sources:
      - ubuntu-toolchain-r-test
    packages:
      - g++-5
  chrome: stable
  hosts:
    - petstore.swagger.io

before_install:
  # install haskell
  - curl -sSL https://get.haskellstack.org/ | sh
  - stack upgrade
  - stack --version
  # install rust
  - curl https://sh.rustup.rs -sSf | sh -s -- -y -v
  # required when sudo: required for the Ruby petstore tests
  - gem install bundler
  - npm install -g typescript
  - npm install -g npm
  - npm install -g elm
  - npm config set registry http://registry.npmjs.org/
  # set python 3.6.3 as default
  - source ~/virtualenv/python3.6/bin/activate
  # to run petstore server locally via docker
  - docker pull swaggerapi/petstore
  - docker run -d -e SWAGGER_HOST=http://petstore.swagger.io -e SWAGGER_BASE_PATH=/v2 -p 80:8080 swaggerapi/petstore
  - docker ps -a
  # Add bats test framework and cURL for Bash script integration tests
  - sudo add-apt-repository ppa:duggan/bats --yes
  - sudo apt-get update -qq
  - sudo apt-get install -qq bats
  - sudo apt-get install -qq curl
  # install cpprest
  - sudo apt-get install libcpprest-dev
  # install perl module
  #- cpanm --local-lib=~/perl5 local::lib && eval $(perl -I ~/perl5/lib/perl5/ -Mlocal::lib)
  #- cpanm Test::Exception Test::More Log::Any LWP::UserAgent JSON URI:Query Module::Runtime DateTime Module::Find Moose::Role
  # comment out below as installation failed in travis
  # Add rebar3 build tool and recent Erlang/OTP for Erlang petstore server tests.
  # - Travis CI does not support rebar3 [yet](https://github.com/travis-ci/travis-ci/issues/6506#issuecomment-275189490).
  # - Rely on `kerl` for [pre-compiled versions available](https://docs.travis-ci.com/user/languages/erlang#Choosing-OTP-releases-to-test-against). Rely on installation path chosen by [`travis-erlang-builder`](https://github.com/travis-ci/travis-erlang-builder/blob/e6d016b1a91ca7ecac5a5a46395bde917ea13d36/bin/compile#L18).
  # - . ~/otp/18.2.1/activate && erl -version
  #- curl -f -L -o ./rebar3 https://s3.amazonaws.com/rebar3/rebar3 && chmod +x ./rebar3 && ./rebar3 version && export PATH="${TRAVIS_BUILD_DIR}:$PATH"

  # show host table to confirm petstore.swagger.io is mapped to localhost
  - cat /etc/hosts
  # show java version
  - java -version
  - if [ "$TRAVIS_BRANCH" = "master" ] && [ "$TRAVIS_PULL_REQUEST" == "false" ]; then
      openssl aes-256-cbc -K $encrypted_6e2c8bba47c6_key -iv $encrypted_6e2c8bba47c6_iv -in sec.gpg.enc -out sec.gpg -d ;
      gpg --keyserver keyserver.ubuntu.com --recv-key $SIGNING_KEY ;
      gpg --check-trustdb ;
    fi;

install:
  # Add Godeps dependencies to GOPATH and PATH
  #- eval "$(curl -sL https://raw.githubusercontent.com/travis-ci/gimme/master/gimme | GIMME_GO_VERSION=1.4 bash)"
  #- export GOPATH="${TRAVIS_BUILD_DIR}/Godeps/_workspace"
  - export PATH="${TRAVIS_BUILD_DIR}/Godeps/_workspace/bin:$HOME/.cargo/bin:$PATH"
  #- go version
  - gcc -v
  - echo $CC
  - echo $CXX

script:
  # fail fast
  - set -e
  # fail if templates/generators contain carriage return '\r'
  - /bin/bash ./bin/utils/detect_carriage_return.sh
  # fail if generators contain merge conflicts
  - /bin/bash ./bin/utils/detect_merge_conflict.sh
  # fail if generators contain tab '\t'
  - /bin/bash ./bin/utils/detect_tab_in_java_class.sh
  # run integration tests defined in maven pom.xml
  - mvn --quiet clean install
  - mvn --quiet verify -Psamples.cpp
after_success:
  # push to maven repo
  - if [ $SONATYPE_USERNAME ] && [ -z $TRAVIS_TAG ] && [ "$TRAVIS_PULL_REQUEST" == "false" ]; then
      if [ "$TRAVIS_BRANCH" = "master" ]; then
        mvn clean deploy -DskipTests=true -B -U -P release --settings CI/settings.xml;
        echo "Finished mvn clean deploy for $TRAVIS_BRANCH";
        pushd .;
        cd modules/openapi-generator-gradle-plugin;
        ./gradlew -Psigning.keyId="$SIGNING_KEY" -Psigning.password="$SIGNING_PASSPHRASE" -Psigning.secretKeyRingFile="${TRAVIS_BUILD_DIR}/sec.gpg" -PossrhUsername="${SONATYPE_USERNAME}" -PossrhPassword="${SONATYPE_PASSWORD}" uploadArchives --no-daemon;
        echo "Finished ./gradlew uploadArchives";
        popd;
      elif ([ "$TRAVIS_BRANCH" == "4.0.x" ]) ; then
        mvn clean deploy --settings CI/settings.xml;
        echo "Finished mvn clean deploy for $TRAVIS_BRANCH";
        pushd .;
        cd modules/openapi-generator-gradle-plugin;
        ./gradlew -PossrhUsername="${SONATYPE_USERNAME}" -PossrhPassword="${SONATYPE_PASSWORD}" uploadArchives --no-daemon;
        echo "Finished ./gradlew uploadArchives";
        popd;
      fi;
    fi;
  ## docker: build and push openapi-generator-online to DockerHub
  - if [ $DOCKER_HUB_USERNAME ]; then echo "$DOCKER_HUB_PASSWORD" | docker login --username=$DOCKER_HUB_USERNAME --password-stdin && docker build -t $DOCKER_GENERATOR_IMAGE_NAME ./modules/openapi-generator-online && if [ ! -z "$TRAVIS_TAG" ]; then docker tag $DOCKER_GENERATOR_IMAGE_NAME:latest $DOCKER_GENERATOR_IMAGE_NAME:$TRAVIS_TAG; fi && if [ ! -z "$TRAVIS_TAG" ] || [ "$TRAVIS_BRANCH" = "master" ]; then docker push $DOCKER_GENERATOR_IMAGE_NAME && echo "Pushed to $DOCKER_GENERATOR_IMAGE_NAME"; fi; fi
  ## docker: build cli image and push to Docker Hub
  - if [ $DOCKER_HUB_USERNAME ]; then echo "$DOCKER_HUB_PASSWORD" | docker login --username=$DOCKER_HUB_USERNAME --password-stdin && docker build -t $DOCKER_CODEGEN_CLI_IMAGE_NAME ./modules/openapi-generator-cli && if [ ! -z "$TRAVIS_TAG" ]; then docker tag $DOCKER_CODEGEN_CLI_IMAGE_NAME:latest $DOCKER_CODEGEN_CLI_IMAGE_NAME:$TRAVIS_TAG; fi && if [ ! -z "$TRAVIS_TAG" ] || [ "$TRAVIS_BRANCH" = "master" ]; then docker push $DOCKER_CODEGEN_CLI_IMAGE_NAME && echo "Pushed to $DOCKER_CODEGEN_CLI_IMAGE_NAME"; fi; fi

env:
  - DOCKER_GENERATOR_IMAGE_NAME=openapitools/openapi-generator-online DOCKER_CODEGEN_CLI_IMAGE_NAME=openapitools/openapi-generator-cli NODE_ENV=test CC=gcc-5 CXX=g++-5
