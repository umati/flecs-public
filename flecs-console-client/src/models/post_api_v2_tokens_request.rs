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
pub struct PostApiV2TokensRequest {
    #[serde(rename = "app")]
    pub app: String,
    #[serde(rename = "version")]
    pub version: String,
}

impl PostApiV2TokensRequest {
    pub fn new(app: String, version: String) -> PostApiV2TokensRequest {
        PostApiV2TokensRequest { app, version }
    }
}
