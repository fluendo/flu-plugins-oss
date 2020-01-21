node('docker-jenkins-ssh-slave') {
    sh "git archive --remote=git@bitbucket.org:fluendo/flu-codec-ci.git master Jenkinsfile | tar -x -O > commonJenkinsfile"
    load 'commonJenkinsfile'
}
