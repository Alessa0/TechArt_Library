using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class LightDataTrans : MonoBehaviour
{
    [SerializeField] Light mainLight;
    [SerializeField] Material material;
    void Start()
    {
        //material.SetVector("_MainLightDirection", mainLight.transform.forward);
        //material.SetColor("_MainLightColor", mainLight.color);
        //Shader.SetGlobalVector("_MainLightDirection", mainLight.transform.forward);
        //Shader.SetGlobalFloat("_MainLightAttenuation", mainLight.range);
        //Shader.SetGlobalColor("_MainLightColor", mainLight.color);
    }
}
