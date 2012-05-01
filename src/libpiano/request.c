/*
Copyright (c) 2008-2012
	Lars-Dominik Braun <lars@6xq.net>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef __FreeBSD__
#define _BSD_SOURCE /* required by strdup() */
#define _DARWIN_C_SOURCE /* strdup() on OS X */
#endif

#include "json/json.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* needed for urlencode */
#include <waitress.h>

#include "piano.h"
#include "piano_private.h"
#include "crypt.h"

/*	prepare piano request (initializes request type, urlpath and postData)
 *	@param piano handle
 *	@param request structure
 *	@param request type
 */
PianoReturn_t PianoRequest (PianoHandle_t *ph, PianoRequest_t *req,
		PianoRequestType_t type) {
	const char *jsonSendBuf;
	const char *method = NULL;
	json_object *j = json_object_new_object ();
	/* corrected timestamp */
	time_t timestamp = time (NULL) - ph->timeOffset;
	bool encrypted = true;

	assert (ph != NULL);
	assert (req != NULL);

	req->type = type;
	/* no tls by default */
	req->secure = false;

	switch (req->type) {
		case PIANO_REQUEST_LOGIN: {
			/* authenticate user */
			PianoRequestDataLogin_t *logindata = req->data;

			assert (logindata != NULL);

			switch (logindata->step) {
				case 0:
					encrypted = false;
					req->secure = true;

					json_object_object_add (j, "username",
							json_object_new_string (ph->partner.user));
					json_object_object_add (j, "password",
							json_object_new_string (ph->partner.password));
					json_object_object_add (j, "deviceModel",
							json_object_new_string (ph->partner.device));
					json_object_object_add (j, "version",
							json_object_new_string ("5"));
					json_object_object_add (j, "includeUrls",
							json_object_new_boolean (true));
					piano_snprintf (req->urlPath, sizeof (req->urlPath),
							PIANO_RPC_PATH "method=auth.partnerLogin");
					break;

				case 1: {
					char *urlencAuthToken;

					req->secure = true;

					json_object_object_add (j, "loginType",
							json_object_new_string ("user"));
					json_object_object_add (j, "username",
							json_object_new_string (logindata->user));
					json_object_object_add (j, "password",
							json_object_new_string (logindata->password));
					json_object_object_add (j, "partnerAuthToken",
							json_object_new_string (ph->partner.authToken));
					json_object_object_add (j, "syncTime",
							json_object_new_int ((int32_t)timestamp));

					urlencAuthToken = WaitressUrlEncode (ph->partner.authToken);
					assert (urlencAuthToken != NULL);
					piano_snprintf (req->urlPath, sizeof (req->urlPath),
							PIANO_RPC_PATH "method=auth.userLogin&"
							"auth_token=%s&partner_id=%i", urlencAuthToken,
							ph->partner.id);
					free (urlencAuthToken);

					break;
				}
			}
			break;
		}

		case PIANO_REQUEST_GET_STATIONS: {
			/* get stations, user must be authenticated */
			assert (ph->user.listenerId != NULL);
			method = "user.getStationList";
			break;
		}

		case PIANO_REQUEST_GET_PLAYLIST: {
			/* get playlist for specified station */
			PianoRequestDataGetPlaylist_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->station != NULL);
			assert (reqData->station->id != NULL);
			assert (reqData->format != PIANO_AF_UNKNOWN);

			req->secure = true;

			json_object_object_add (j, "stationToken",
					json_object_new_string (reqData->station->id));

			method = "station.getPlaylist";
			break;
		}

		case PIANO_REQUEST_ADD_FEEDBACK: {
			/* low-level, don't use directly (see _RATE_SONG and _MOVE_SONG) */
			PianoRequestDataAddFeedback_t *reqData = req->data;
			
			assert (reqData != NULL);
			assert (reqData->trackToken != NULL);
			assert (reqData->rating != PIANO_RATE_NONE);

			json_object_object_add (j, "trackToken",
					json_object_new_string (reqData->trackToken));
			json_object_object_add (j, "isPositive",
					json_object_new_boolean (reqData->rating == PIANO_RATE_LOVE));

			method = "station.addFeedback";
			break;
		}

		case PIANO_REQUEST_RENAME_STATION: {
			PianoRequestDataRenameStation_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->station != NULL);
			assert (reqData->newName != NULL);

			json_object_object_add (j, "stationToken",
					json_object_new_string (reqData->station->id));
			json_object_object_add (j, "stationName",
					json_object_new_string (reqData->newName));

			method = "station.renameStation";
			break;
		}

		case PIANO_REQUEST_DELETE_STATION: {
			/* delete station */
			PianoStation_t *station = req->data;

			assert (station != NULL);
			assert (station->id != NULL);

			json_object_object_add (j, "stationToken",
					json_object_new_string (station->id));

			method = "station.deleteStation";
			break;
		}

		case PIANO_REQUEST_SEARCH: {
			/* search for artist/song title */
			PianoRequestDataSearch_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->searchStr != NULL);

			json_object_object_add (j, "searchText",
					json_object_new_string (reqData->searchStr));

			method = "music.search";
			break;
		}

		case PIANO_REQUEST_CREATE_STATION: {
			/* create new station from specified musicid (type=mi, get one by
			 * performing a search) or shared station id (type=sh) */
			PianoRequestDataCreateStation_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->id != NULL);

			json_object_object_add (j, "musicToken",
					json_object_new_string (reqData->id));

			method = "station.createStation";
			break;
		}

		case PIANO_REQUEST_ADD_SEED: {
			/* add another seed to specified station */
			PianoRequestDataAddSeed_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->station != NULL);
			assert (reqData->musicId != NULL);

			json_object_object_add (j, "musicToken",
					json_object_new_string (reqData->musicId));
			json_object_object_add (j, "stationToken",
					json_object_new_string (reqData->station->id));

			method = "station.addMusic";
			break;
		}

		case PIANO_REQUEST_ADD_TIRED_SONG: {
			/* ban song for a month from all stations */
			PianoSong_t *song = req->data;

			assert (song != NULL);

			json_object_object_add (j, "trackToken",
					json_object_new_string (song->trackToken));

			method = "user.sleepSong";
			break;
		}

		case PIANO_REQUEST_SET_QUICKMIX: {
			/* select stations included in quickmix (see useQuickMix flag of
			 * PianoStation_t) */
			PianoStation_t *curStation = ph->stations;
			json_object *a = json_object_new_array ();

			while (curStation != NULL) {
				/* quick mix can't contain itself */
				if (curStation->useQuickMix && !curStation->isQuickMix) {
					json_object_array_add (a,
							json_object_new_string (curStation->id));
				}

				curStation = curStation->next;
			}

			json_object_object_add (j, "quickMixStationIds", a);

			method = "user.setQuickMix";
			break;
		}

		case PIANO_REQUEST_GET_GENRE_STATIONS: {
			/* receive list of pandora's genre stations */
			method = "station.getGenreStations";
			break;
		}

		case PIANO_REQUEST_TRANSFORM_STATION: {
			/* transform shared station into private */
			PianoStation_t *station = req->data;

			assert (station != NULL);

			json_object_object_add (j, "stationToken",
					json_object_new_string (station->id));

			method = "station.transformSharedStation";
			break;
		}

		case PIANO_REQUEST_EXPLAIN: {
			/* explain why particular song was played */
			PianoRequestDataExplain_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->song != NULL);

			json_object_object_add (j, "trackToken",
					json_object_new_string (reqData->song->trackToken));

			method = "track.explainTrack";
			break;
		}

		case PIANO_REQUEST_GET_SEED_SUGGESTIONS: {
#if 0
			/* find similar artists */
			PianoRequestDataGetSeedSuggestions_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->musicId != NULL);
			assert (reqData->max != 0);

			snprintf (xmlSendBuf, sizeof (xmlSendBuf), "<?xml version=\"1.0\"?>"
					"<methodCall><methodName>music.getSeedSuggestions</methodName>"
					"<params><param><value><int>%lu</int></value></param>"
					/* auth token */
					"<param><value><string>%s</string></value></param>"
					/* station id */
					"<param><value><string>%s</string></value></param>"
					/* seed music id */
					"<param><value><string>%s</string></value></param>"
					/* max */
					"<param><value><int>%u</int></value></param>"
					"</params></methodCall>", (unsigned long) timestamp,
					ph->user.authToken, reqData->station->id, reqData->musicId,
					reqData->max);
			snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
					"rid=%s&lid=%s&method=getSeedSuggestions&arg1=%s&arg2=%u",
					ph->routeId, ph->user.listenerId, reqData->musicId, reqData->max);
#endif
			break;
		}

		case PIANO_REQUEST_BOOKMARK_SONG: {
			/* bookmark song */
			PianoSong_t *song = req->data;

			assert (song != NULL);

			json_object_object_add (j, "trackToken",
					json_object_new_string (song->trackToken));

			method = "bookmark.addSongBookmark";
			break;
		}

		case PIANO_REQUEST_BOOKMARK_ARTIST: {
			/* bookmark artist */
			PianoSong_t *song = req->data;

			assert (song != NULL);

			json_object_object_add (j, "trackToken",
					json_object_new_string (song->trackToken));

			method = "bookmark.addArtistBookmark";
			break;
		}

		case PIANO_REQUEST_GET_STATION_INFO: {
			/* get station information (seeds and feedback) */
			PianoRequestDataGetStationInfo_t *reqData = req->data;

			assert (reqData != NULL);
			assert (reqData->station != NULL);

			json_object_object_add (j, "stationToken",
					json_object_new_string (reqData->station->id));
			json_object_object_add (j, "includeExtendedAttributes",
					json_object_new_boolean (true));

			method = "station.getStation";
			break;
		}

		case PIANO_REQUEST_DELETE_FEEDBACK: {
			PianoSong_t *song = req->data;

			assert (song != NULL);

			json_object_object_add (j, "feedbackId",
					json_object_new_string (song->feedbackId));

			method = "station.deleteFeedback";
			break;
		}

		case PIANO_REQUEST_DELETE_SEED: {
			PianoRequestDataDeleteSeed_t *reqData = req->data;
			char *seedId = NULL;

			assert (reqData != NULL);
			assert (reqData->song != NULL || reqData->artist != NULL ||
					reqData->station != NULL);

			if (reqData->song != NULL) {
				seedId = reqData->song->seedId;
			} else if (reqData->artist != NULL) {
				seedId = reqData->artist->seedId;
			} else if (reqData->station != NULL) {
				seedId = reqData->station->seedId;
			}

			assert (seedId != NULL);

			json_object_object_add (j, "seedId",
					json_object_new_string (seedId));

			method = "station.deleteMusic";
			break;
		}

		/* "high-level" wrapper */
		case PIANO_REQUEST_RATE_SONG: {
			/* love/ban song */
			PianoRequestDataRateSong_t *reqData = req->data;
			PianoReturn_t pRet;
			PianoRequestDataAddFeedback_t transformedReqData;

			assert (reqData != NULL);
			assert (reqData->song != NULL);
			assert (reqData->rating != PIANO_RATE_NONE);

			transformedReqData.stationId = reqData->song->stationId;
			transformedReqData.trackToken = reqData->song->trackToken;
			transformedReqData.rating = reqData->rating;
			req->data = &transformedReqData;

			/* create request data (url, post data) */
			pRet = PianoRequest (ph, req, PIANO_REQUEST_ADD_FEEDBACK);
			/* and reset request type/data */
			req->type = PIANO_REQUEST_RATE_SONG;
			req->data = reqData;

			return pRet;
			break;
		}

		case PIANO_REQUEST_MOVE_SONG: {
			/* move song to a different station, needs two requests */
			PianoRequestDataMoveSong_t *reqData = req->data;
			PianoRequestDataAddFeedback_t transformedReqData;
			PianoReturn_t pRet;

			assert (reqData != NULL);
			assert (reqData->song != NULL);
			assert (reqData->from != NULL);
			assert (reqData->to != NULL);
			assert (reqData->step < 2);

			transformedReqData.trackToken = reqData->song->trackToken;
			req->data = &transformedReqData;

			switch (reqData->step) {
				case 0:
					transformedReqData.stationId = reqData->from->id;
					transformedReqData.rating = PIANO_RATE_BAN;
					break;

				case 1:
					transformedReqData.stationId = reqData->to->id;
					transformedReqData.rating = PIANO_RATE_LOVE;
					break;
			}

			/* create request data (url, post data) */
			pRet = PianoRequest (ph, req, PIANO_REQUEST_ADD_FEEDBACK);
			/* and reset request type/data */
			req->type = PIANO_REQUEST_MOVE_SONG;
			req->data = reqData;

			return pRet;
			break;
		}
	}

	/* standard parameter */
	if (method != NULL) {
		char *urlencAuthToken;

		assert (ph->user.authToken != NULL);

		urlencAuthToken = WaitressUrlEncode (ph->user.authToken);
		assert (urlencAuthToken != NULL);

		piano_snprintf (req->urlPath, sizeof (req->urlPath), PIANO_RPC_PATH
				"method=%s&auth_token=%s&partner_id=%i&user_id=%s", method,
				urlencAuthToken, ph->partner.id, ph->user.listenerId);

		free (urlencAuthToken);

		json_object_object_add (j, "userAuthToken",
				json_object_new_string (ph->user.authToken));
		json_object_object_add (j, "syncTime",
				json_object_new_int ((int32_t)timestamp));
	}

	/* json to string */
	jsonSendBuf = json_object_to_json_string (j);
	if (encrypted) {
		if ((req->postData = PianoEncryptString (&ph->partner.out,
				jsonSendBuf)) == NULL) {
			return PIANO_RET_OUT_OF_MEMORY;
		}
	} else {
		req->postData = strdup (jsonSendBuf);
	}
	json_object_put (j);

	return PIANO_RET_OK;
}
