/**
 * Copyright (c) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You may
 * obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

'use strict';

const GERRIT_REVIEW_URL = 'https://android-review.googlesource.com/c/platform/external/perfetto';
const GERRIT_URL = '/changes/?q=project:platform/external/perfetto+-age:7days&o=DETAILED_ACCOUNTS';
const TRAVIS_URL = 'https://api.travis-ci.org';
const TRAVIS_REPO = 'primiano/perfetto-ci';

var botIndex = {};

function GetColumnIndexes() {
  const cols = document.getElementById('cls_header').children;
  for (var i = 0; i < cols.length; i++) {
    const id = cols[i].id;
    if (id)
      botIndex[id] = i;
  }
}

function GetTravisStatusForJob(jobId, div) {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (this.readyState != 4 || this.status != 200)
      return;
    var resp = JSON.parse(this.responseText);
    var jobName = resp.config.env.split(' ')[0];
    if (jobName.startsWith('CFG='))
      jobName = jobName.substring(4);
    var link = document.createElement('a');
    link.href = 'https://travis-ci.org/' + TRAVIS_REPO + '/jobs/' + jobId;
    link.title = resp.state + ' [' + jobName + ']';
    if (resp.state == 'finished')
      link.innerHTML = '&#x2705;';
    else if (resp.state == 'errored')
      link.innerHTML = '&#x274C;';
    else
      link.innerHTML = '&#x27B2;';
    var td = div.children[botIndex[jobName]];
    td.innerHTML = '';
    td.appendChild(link);
    td.classList.add('job');
    td.classList.add(resp.state);
  };
  xhr.open('GET', TRAVIS_URL + '/jobs/' + jobId, true);
  xhr.send();
}

function GetTravisStatusForCL(clNum, div) {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 404) {
      return;
    }
    if (this.readyState != 4 || this.status != 200)
      return;
    var resp = JSON.parse(this.responseText);
    for (const jobId of resp.branch.job_ids)
      GetTravisStatusForJob(jobId, div);
  };
  var url = TRAVIS_URL + '/repos/' + TRAVIS_REPO + '/branches/changes/' + clNum;
  xhr.open('GET', url, true);
  xhr.send();
}

function LoadGerritCLs() {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (this.readyState != 4 || this.status != 200)
      return;
    var resp = this.responseText;
    if (resp.startsWith(')]}\''))
      resp = resp.substring(4);
    var resp = JSON.parse(resp);
    var table = document.getElementById('cls');
    for (const cl of resp) {
      var tr = document.createElement('tr');

      var link = document.createElement('a');
      link.href = GERRIT_REVIEW_URL + '/+/' + cl._number;
      link.innerText = cl.subject;
      var td = document.createElement('td');
      td.appendChild(link);
      tr.appendChild(td);

      td = document.createElement('td');
      td.innerText = cl.status;
      tr.appendChild(td);

      td = document.createElement('td');
      td.innerText = cl.owner.email.replace('@google.com', '@');
      tr.appendChild(td);

      td = document.createElement('td');
      const lastUpdate = new Date(cl.updated);
      const lastUpdateMins = Math.ceil((Date.now() - lastUpdate) / 60000);
      if (lastUpdateMins < 60)
        td.innerText = lastUpdateMins + ' mins ago';
      else if (lastUpdateMins < 60 * 24)
        td.innerText = Math.ceil(lastUpdateMins/60) + ' hours ago';
      else
        td.innerText = (new Date(cl.updated)).toLocaleDateString();
      tr.appendChild(td);

      for (var _ in botIndex)
        tr.appendChild(document.createElement('td'));

      GetTravisStatusForCL(cl._number, tr);

      table.appendChild(tr);
      // console.log(cl);
    }
  };
  xhr.open('GET', GERRIT_URL, true);
  xhr.send();
}

GetColumnIndexes();
LoadGerritCLs();
