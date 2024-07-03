/*
 * FLECS Console API
 *
 * No description provided (generated by Openapi Generator https://github.com/openapitools/openapi-generator)
 *
 * The version of the OpenAPI document: 1.0.0
 *
 * Generated by: https://openapi-generator.tech
 */

use crate::models;
use serde::{Deserialize, Serialize};

#[derive(Clone, Default, Debug, PartialEq, Serialize, Deserialize)]
pub struct Apiv2AuthLoginPostRequest {
    #[serde(rename = "issueJWT", skip_serializing_if = "Option::is_none")]
    pub issue_jwt: Option<bool>,
    #[serde(rename = "password", skip_serializing_if = "Option::is_none")]
    pub password: Option<String>,
    #[serde(rename = "username", skip_serializing_if = "Option::is_none")]
    pub username: Option<String>,
}

impl Apiv2AuthLoginPostRequest {
    pub fn new() -> Apiv2AuthLoginPostRequest {
        Apiv2AuthLoginPostRequest {
            issue_jwt: None,
            password: None,
            username: None,
        }
    }
}
